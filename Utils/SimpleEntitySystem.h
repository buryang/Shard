#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/Memory.h"
#include "Utils/Algorithm.h"
#include "Utils/SimpleJobSystem.h"
#include "Delegate/Delegate/MulticastDelegate.h"
#include <limits>
#include <type_traits>
#include <typeindex>

/**
 * Core Design and Features of this ECS Framework
 *
 * 1. Hybrid Storage Architecture
 *    - Dense storage: Classic Archetype + Chunk model (similar to Unity DOTS, Bevy, Flecs tables)
 *      Ideal for high-frequency, batch-accessed components with contiguous, cache-friendly SOA layout.
 *    - Sparse storage: Fixed maximum of 64 SparseSet types for rare/optional/tag components.
 *
 * 2. SparseSet Capacity and Implementation
 *    - Maximum 64 SparseSet component types (hard limit)
 *    - ComponentIDs in range [0, 63] are treated as SparseSet components
 *      - ID directly used as bit position in EntityLocation::sparse_mask (uint64_t)
 *      - Each SparseSet stored in sparse_stores[64] array
 *    - Enables O(1) "has component" checks via single bitwise AND
 *
 * 3. Singleton Components
 *    - Implemented by reusing SparseSet mechanism on a single fixed global Entity (usually ID 0)
 *    - Each Singleton occupies one slot in the 0~63 range
 *    - Total number of Singletons + regular Sparse components ¡Ü 64
 *    - Access is identical to regular SparseSet (via fixed Entity + mask check)
 *    - Advantage: unified code path, no extra global storage
 *
 * 4. Query System Capabilities
 *    - Supports hybrid queries: Dense (required) + Sparse (optional) components
 *      - Dense filters archetypes/chunks
 *      - Sparse checked per-entity via sparse_mask (O(1))
 *    - Supports pure-Sparse queries (no required dense components)
 *      - Switches to SparseSet iteration mode automatically
 *      - Uses one main Sparse component as iteration source, others filtered by presence
 *    - Cache + dirty mechanism: Uses World version counter to detect structural changes
 *      and automatically rebuild matching archetype list when dirty
 *
 * Design Trade-offs:
 * - Performance & simplicity prioritized: Hard 64 limit on Sparse/Singleton for bit-level efficiency
 * - Singletons reuse SparseSet: avoids separate global singleton storage
 * - Query flexibility: covers both mixed dense-sparse and pure-sparse use cases
 */

//todo realize wildcard, entity as component

#define ARCHETYPE_BLOOM_FILTER_ENABLED
//enable bit pools described in https://jakubtomsu.github.io/posts/bit_pools/
//which is a more memory efficient & less cache miss way, when your entity number is huge enable it
//i think it just share a list-like pointer(vector index) by a few entity and use bit to sign whether enitty is used to save memory
#define ENTITY_MANAGER_BIT_POOL_ENABLED 

namespace Shard
{
    namespace Utils
    { 
        //basic types 
        using EntityIndex = uint32_t;
        using EntityVersion = uint32_t;
        using ComponentID = uint32_t;
        using ArcheTypeID = uint32_t;
        using Ticks = uint64_t;

        constexpr EntityIndex INVALID_ENTITY_INDEX = ~0u;
        constexpr EntityVersion INVALID_ENTITY_VERSION = ~0u;
        constexpr ComponentID INVALID_COMPONENT_ID = ~0u;
        /*sparseset type component id 0~63, and singleton component is a special sparseset component, 
        * so they share the id 0~63£¬ that mean u should not have more than 64 singleton/sparseset component, 
        * but most game never need more than 64 sparsecomponents, and singleton component is not so much */
        constexpr ComponentID MAX_SPARSE_COMPONENT_ID = 63u; //most game never need more than 64 sparse components
        constexpr ComponentID MIN_DENSE_COMPONENT_ID = MAX_SPARSE_COMPONENT_ID + 1u;
        constexpr ArcheTypeID INVALID_ARCHETYPE_ID = ~0u;

        //bloom filter expected number of query 
        constexpr size_type QUEY_BLOOM_EXPECTN = 32u;
        //bloom filter expected number of archetype
        constexpr size_type ARCHETYPE_BLOOM_EXPECTN = 64u;

        template<typename, typename = void>
        struct has_update_func : std::false_type {};
        template<typename System>
        struct has_update_func<System, std::void_t<decltype(std::declval<System>.Update())>> : std::true_type {};
        template<typename, typename, typename = void>
        struct has_message : std::false_type {};
        template<typename System, typename MessageType>
        struct has_message<System, MessageType, std::void_t<decltype(std::declval<System>().Deal(std::declval<MessageType>()))> > : std::true_type {};
        template<typename, typename = void>
        struct has_component : std::false_type{};
        template<typename System>
        struct has_component<System, std::void_t<decltype(std::declval<System>().components_)>> : std::true_type {};
        //component system operation
        
        template<typename, typename>
        struct is_applicable : std::false_type {};
        template<typename Func, template<typename...> class Tuple, typename... Args>
        struct is_applicable<Func, const Tuple<Args...>> : std::is_invocable<Func, Args...> {};

        //component delegate check
#define DECLARE_COMPONENT_DELEGATE(Delegate)\
        template<typename ComponentRepo, typename = void> \
        struct has_##Delegate : std::false_type {}; \
        template<typename ComponentRepo, typename Delegate> \
        struct has_##Delegate<ComponentRepo, std::void_t<decltype(std::declval<ComponentRepo>().Delegate)>> : std::true_type {}; 

        DECLARE_COMPONENT_DELEGATE(OnConstruct)
        DECLARE_COMPONENT_DELEGATE(OnUpdate)
        DECLARE_COMPONENT_DELEGATE(OnRelease)
#undef DECLARE_COMPONENT_DELEGATE

        struct Entity
        {
            Entity(uint32_t id = -1, uint32_t gen = 0u) :id_(id), generation_(gen) {}
            uint32_t Index() const { return id_; }
            uint32_t Generation() const { return generation_; }
            bool operator==(const Entity & rhs) const{
                return id_ == rhs.id_ && generation_ == rhs.generation_;
            }
            bool operator!=(const Entity& rhs) const {
                return !(*this == rhs);
            }
            bool IsValid() const {
                return generation_ > 0 && index_ != INVALID_ENTITY_INDEX;
            }
            explicit operator bool() const {
                return IsValid();
            }
            uint32_t    id_ : 20;
            uint32_t    generation_ : 12;
            //fixme https://ajmmertens.medium.com/doing-a-lot-with-a-little-ecs-identifiers-25a72bd2647 
            static const Entity Null;
        };

        class ArcheTypeTable;

        struct EntityLocation
        {
            ArcheTypeTable* arche_type_{ nullptr };
            uint32_t chunk_index_{ ~0u };
            uint32_t slot_in_chunk_{ ~0u };
            //EntityVersion generation_;

            //sparseset componnet mask, current 64 sparseset components 
            uint64_t sparse_mask_{ 0u};

            bool IsValid() const
            {
                return arche_type_ != nullptr && chunk_index_ != ~0u && slot_in_chunk_ != ~0u;
            }
        };

        inline bool TestEntitySparse(const EntityLocation& loc)
        {
            return loc.sparse_mask_ != 0u;
        }

        inline bool TestEntitySparseBit(const EntityLocation& loc, uint32_t bit)
        {
            return (loc.sparse_mask_ & (1ULL << bit)) != 0u;
        }

        inline void SetEntitySparseBit(EntityLocation& loc, uint32_t bit)
        {
            loc.sparse_mask_ |= (1ULL << bit);
        }

        inline void ClearEntitySparseBit(EntityLocation& loc, uint32_t bit)
        {
            loc.sparse_mask_ &= ~(1ULL << bit);
        }
        
        //entity manager, manage entity id generation
#ifndef ENTITY_MANAGER_BIT_POOL_ENABLED
        class EntityManager final
        {
        public:
            using VersionType = uint16_t;
            EntityManager() = default;
            EntityManager(const EntityManager& other);
            EntityManager& operator=(const EntityManager& other);
            Entity Spawn();
            /*inset an user input entity id */
            bool Insert(const Entity& entity);
            void Release(Entity entity);
            VersionType Version(const Entity& entity)const;
            bool IsAlive(const Entity& entity) const;
            const Vector<Entity>& GetAliveEntities()const;

            //location
            EntityLocation GetLocation(Entity entity)const;
            void SetLocation(Entity entity, const Entity& loc);
        public:
            Vector<VersionType>    generation_;
            List<uint32_t>    free_indices_; //entity is 4bytes, smaller than list pointer; waste of memory(for huge number of entity) but simple and fast

            //entity location size equal to generation size
            Vector<EntityLocation> locations_;
        };
#else
        class EntityManager final
        {
        public:
            using VersionType = uint16_t;

            EntityManager() = default;
            EntityManager(const EntityManager& other);
            EntityManager& operator=(const EntityManager& other);

            Entity Spawn();
            bool Insert(const Entity& entity);
            void Release(Entity entity);
            VersionType Version(const Entity& entity) const;
            bool IsAlive(const Entity& entity) const;
            const Vector<Entity>& GetAliveEntities() const;

            // location
            EntityLocation GetLocation(Entity entity) const;
            void SetLocation(Entity entity, const EntityLocation& loc);

        private:
            //bit-packed free list configuration
            static constexpr size_t PAGE_CAPACITY = 4096u;          // slots per page
            static constexpr uint32_t INVALID_INDEX = ~0u;
            static constexpr uint16_t MAX_GENERATION = 0xFFFF;

            struct FreePage {
                alignas(64) uint64_t free_bits_[PAGE_CAPACITY / 64]{}; //tdo change to atomic
                uint32_t allocated_ = 0u;
                uint32_t first_free_hint_ = 0u;  //cache last known free position

                uint32_t FindFreeSlot() noexcept {
                    //start from hint (usually very close to previous free)
                    size_t w_start = first_free_hint_ / 64;
                    size_t b_start = first_free_hint_ % 64;

                    // Scan from hint to end of page
                    for (size_t w = w_start; w < (PAGE_CAPACITY / 64); ++w) {
                        uint64_t word = ~free_bits_[w];  // 1 = free
                        if (word != 0) {
                            size_t bit = count_zero(word);
                            if (w == w_start && bit < b_start) continue;  // skip before hint
                            uint32_t idx = static_cast<uint32_t>(w * 64 + bit);
                            first_free_hint_ = idx + 1;
                            return idx;
                        }
                    }

                    //wrap around from beginning to hint
                    for (size_t w = 0; w < w_start; ++w) {
                        uint64_t word = ~free_bits_[w];
                        if (word != 0) {
                            size_t bit = count_zero(word);
                            uint32_t idx = static_cast<uint32_t>(w * 64 + bit);
                            first_free_hint_ = idx + 1;
                            return idx;
                        }
                    }
                    return PAGE_CAPACITY;
                }

                void MarkUsed(uint32_t idx) noexcept {
                    assert(idx < PAGE_CAPACITY);
                    size_t w = idx / 64;
                    size_t b = idx % 64;
                    free_bits_[w] |= (1ULL << b);
                    ++allocated_;
                }

                void MarkFree(uint32_t idx) noexcept {
                    assert(idx < PAGE_CAPACITY);
                    size_t w = idx / 64;
                    size_t b = idx % 64;
                    free_bits_[w] &= ~(1ULL << b);
                    --allocated_;
                }

                bool HasFree() const noexcept { return allocated_ < PAGE_CAPACITY; }
            };

            Vector<VersionType>    generation_;
            Vector<EntityLocation> locations_;

            // Bit-packed free list
            Vector<eastl::unique_ptr<FreePage>> free_pages_;
            Vector<uint32_t>                  pages_with_free_;  // indices of pages that have free slots

            //track total alive entities (for GetAliveEntities if needed)
            //Vector<Entity> alive_entities_;  // maintained only if you really need it
        private:
            uint32_t AllocateSlot() {
                //try to reuse from pages with free slots
                while (!pages_with_free_.empty()) {
                    uint32_t page_idx = pages_with_free_.back();
                    auto& page = *free_pages_[page_idx];

                    uint32_t slot = page.FindFreeSlot();
                    if (slot < PAGE_CAPACITY) {
                        page.MarkUsed(slot);
                        if (!page.HasFree()) {
                            pages_with_free_.pop_back();
                        }
                        return page_idx * PAGE_CAPACITY + slot;
                    }
                    // Page unexpectedly full ¡ú remove it
                    pages_with_free_.pop_back();
                }

                //need a new page
                uint32_t page_idx = static_cast<uint32_t>(free_pages_.size());
                auto page = eastl::make_unique<FreePage>();
                uint32_t slot = 0;
                page->MarkUsed(slot);

                free_pages_.push_back(std::move(page));
                pages_with_free_.push_back(page_idx);

                //grow generation_ and locations_ if needed
                uint32_t global_id = page_idx * PAGE_CAPACITY + slot;
                if (global_id >= generation_.size()) {
                    size_t new_size = global_id + PAGE_CAPACITY;
                    generation_.resize(new_size, 0);
                    locations_.resize(new_size);
                }

                return global_id;
            }

            void FreeSlot(uint32_t global_id) {
                uint32_t page_idx = global_id / PAGE_CAPACITY;
                uint32_t slot = global_id % PAGE_CAPACITY;

                if (page_idx >= free_pages_.size()) return;

                auto& page = *free_pages_[page_idx];
                page.MarkFree(slot);

                // If page now has free slots, add to free list
                if (page.allocated_ == PAGE_CAPACITY - 1) {
                    pages_with_free_.push_back(page_idx);
                }
            }

        public:
            Entity Spawn() {
                uint32_t id = AllocateSlot();

                auto& gen = generation_[id];
                gen = (gen + 1) & MAX_GENERATION;

                //reset location
                locations_[id] = EntityLocation{};  // your default/zero value

                return Entity{ id, gen };
            }

            bool Insert(const Entity& entity) {
                if (entity.id_ >= generation_.size()) return false;

                auto& gen = generation_[entity.id_];
                if (gen != entity.generation_) return false;  //stale

                // Already alive? (optional check)
                if (IsAlive(entity)) return false;

                gen = entity.generation_;
                locations_[entity.id_] = EntityLocation{};  // reset or keep previous?

                return true;
            }

            void Release(Entity entity) {
                if (entity.id_ >= generation_.size()) return;

                auto& gen = generation_[entity.id_];
                if (gen != entity.generation_) return;  //stale

                //cleanup location
                locations_[entity.id_] = EntityLocation{};

                //recycle slot
                FreeSlot(entity.id_);
            }

            VersionType Version(const Entity& entity) const {
                if (entity.id_ >= generation_.size()) return 0;
                return generation_[entity.id_];
            }

            bool IsAlive(const Entity& entity) const {
                if (entity.id_ >= generation_.size()) return false;
                const auto& entry = locations_[entity.id_];
                return generation_[entity.id_] == entity.generation_;
            }

            const Vector<Entity>& GetAliveEntities() const {
                
                static Vector<Entity> dummy;
                return dummy;  // replace with real implementation if needed
            }

            EntityLocation GetLocation(Entity entity) const {
                if (entity.id_ >= locations_.size()) return {};
                return locations_[entity.id_];
            }

            void SetLocation(Entity entity, EntityLocation&& loc) {
                if (entity.id_ >= locations_.size()) return;
                locations_[entity.id_] = std::move(loc);
            }
        };
#endif 

        enum class EComponentStorageType :uint8_t
        {
            eDense, //dense data component, stored in contiguous memory for better cache performance, suitable for frequently accessed component
            eSparse, //sparse data component, stored in hash map or other sparse data structure, suitable for infrequently accessed component
            eAuto,   //auto select based on entity count
            eDefault = eDense,
        };

#define COMPONENT_FLAG_IS_SINGTON 0x1
#define COMPONENT_FLAG_IS_TAG     0x2

        struct ComponentTypeInfo 
        {
            std::type_index type_index_;
            size_type size_;
            size_type align_;
            //todo construct/destruct func pointer      
            uint8_t flags_; 

            template<class Component>
            static ComponentTypeInfo Create() {
                ComponentTypeInfo type_info;
                std::type_index_ = std::type_index(typeid(Component));
                info.size_ = sizeof(Component);
                info.alignment_ = alignof(Component);

                //todo construct/destruct
                return type_info;
            }
        };

        inline bool IsSparseComponent(ComponentID id) {
            return id <= MAX_SPARSE_COMPONENT_ID; //bit trick? !(id & ~63u);
        }

        inline bool IsDenseComponent(ComponentID id) {
            return id > MAX_SPARSE_COMPONENT_ID;
        }

        //realize singleton component as a special case of sparse component 
        inline bool IsSingletonComponent(ComponentID id, const ComponentTypeInfo& info)
        {
            return IsSparseComponent(id) && (info.flags_ & COMPONENT_FLAG_IS_SINGTON) != 0u;
        }

        /*
        * because performance reason, we donnot use hashing for ID
        * instead using counter & LUT to search id, now we use 0~63 as sparse 
        * component id, which can directly use as index to sparse comonent repo
        * and EntityLocation sparse 64bit mask
        */
        class ComponentTypeRegistry final
        {
            HashMap<std::type_index, ComponentID> type_to_id_;
            HashMap<ComponentID, ComponentTypeInfo> id_to_info_;
            ComponentID curr_dense_id_{ MIN_DENSE_COMPONENT_ID };
            ComponentID curr_sparse_id_{ 0u };

        public:
            ComponentID RegisterComponent(const std::type_index& type, const ComponentTypeInfo& info, EComponentStorageType storage_type)
            {
                auto iter = type_to_id_.find(type);
                if (iter != type_to_id_.end()) {
                    return iter->second;
                }
                ComponentID id = INVALID_COMPONENT_ID;
                if (storage_type == EComponentStorageType::eDense)
                {
                    id = curr_dense_id_++;
                }
                else
                {
                    id = curr_sparse_id_++;
                    if (id > MAX_SPARSE_COMPONENT_ID)
                    {
                        LOG(ERROR) << "you add too much sparse component, now only support max count 64";
                        return INVALID_COMPONENT_ID;
                    }
                }

                type_to_id_[type] = id;
                id_to_info_[id] = info;
                return id;
            }
            template<class T>
            ComponentID RegisterComponent() {
                auto type_index = std::type_index(typeid(T));
                auto type_info = ComponentTypeInfo::Create<T>();
                return RegisterComponent(type_index, type_info);
            }

            ComponentID GetComponentID(const std::type_index& type) const
            {
                auto iter = type_to_id_.find(type);
                if (iter != type_to_id_.end()) {
                    return iter->second;
                }
                return INVALID_COMPONENT_ID;
            }

            const ComponentTypeInfo* GetComponentTypeInfo(ComponentID id) const
            {
                auto iter = id_to_info_.find(id);
                if (iter != id_to_info_.end()) {
                    return &iter->second;
                }
                return nullptr;
            }

            const ComponentTypeInfo* GetComponentTypeInfo(const std::type_index& type) const
            {
                if (auto id = GetComponentID(type); id != INVALID_COMPONENT_ID)
                {
                    auto iter = id_to_info_.find(id);
                    if (iter != id_to_info_.end()) {
                        return &iter->second;
                    }
                }
                return nullptr;
            }

            template<class Component>
            ComponentID GetComponentID() const {
                return GetComponentID(typeid(Component));
            }

            template<class Component>
            const ComponentTypeInfo* GetComponentTypeInfo() const
            {
                return GetComponentTypeInfo(typeid(Ccomponenet));
            }
        };


        template<typename Container>
        class SparseSetIterator
        {
        public:
            using value_type = typename Container::value_type;
            using pointer = typename Container::pointer;
            using difference_type = typename Container::difference_type;
            using iterator_category = std::random_access_iterator_tag;
        public:
            SparseSetIterator() = default;
            SparseSetIterator(const Container& container, const difference_type index):container_(std::addressof(container)), offset_(index) {

            }
            value_type operator*()const {
                return *operator->();
            }
            pointer operator->()const{
                &container_[offset_];
            }
            SparseSetIterator& operator++() {
                ++offset_;
                return *this;
            }
            SparseSetIterator operator++(int) {
                auto temp = *this;
                ++(*this);
                return temp;
            }
            friend bool operator==(const SparseSetIterator& lhs, const SparseSetIterator& rhs) {
                return lhs.offset_ == rhs.offset_;
            }
            friend bool operator!=(const SparseSetIterator& lhs, const SparseSetIterator& rhs) {
                return !(lhs == rhs);
            }
        private:
            const Container* container_{ nullptr };
            difference_type    offset_{ 0u };
        };

        template<typename ElementType, template <typename> typename AllocatorType>
        class SparseSet final
        {
        public:
            using ValueType = ElementType;
            using Allocator = AllocatorType<ValueType>;
            using ThisType = SparseSet<ValueType, AllocatorType>;
            using SizeType = Vector<ValueType>::size_type;
            using Iterator = SparseSetIterator<Vector<ValueType>>;
            using ConstIterator = Vector<ValueType>::const_iterator;
       
            SparseSet() = default;
            explicit SparseSet(SizeType size, Allocator& alloc) : dense_(alloc), page_alloc_(alloc) {
                Resize(size);
            }
            void Resize(SizeType size) { //deprecated todo
                const auto old_size = Count();
                if (old_size > size) {
                    //to do
                }
                dense_.resize(size);
                const auto page_size = std::ceil(float(old_size) / SparsePage::PAGE_SIZE);
                //todo renew pages
            }
            Iterator Begin() {
                return Iterator(dense_, 0u);
            }
            Iterator End() {
                return Iterator(dense_, dense_.size());
            }
            void Swap(ThisType& rhs) {
                std::swap(dense_, rhs.dense_);
                std::swap(sparse_pages_, rhs.sparse_pages_);
                std::swap(page_alloc_ rhs.page_alloc_);
                //todo allocator
            }
            void SwapByDense(ValueType lhs, ValueType rhs) {
                SetSparseValue(dense_[lhs], rhs);
                SetSparseValue(dense_[rhs], lhs);
                std::swap(dense_[lhs], dense_[rhs]);
            }
            void Insert(ValueType i) {
                dense_.push_back(i);
                SetSparseValue(i, dense_.size() - 1);
            }
            void Erase(ValueType i) {
                const auto dense_index = GetSparseValue(i);
                sparse_[i] = -1;
                if (dense_index != dense_, size() - 1) {
                    const auto dense_back = dense_.back();
                    sparse_[dense_back] = dense_index; //todo
                    dense_[dense_index] = dense_back;//todo
                }
                dense_.pop_back();
            }
            void Erase(const Iterator& iter) {
                Erase(*iter);
            }
            void Clear() {
                dense_.clear();
                for (auto* page : sparse_pages_) {
                    page_alloc_.DeAlloc(page);
                    page_ = nullptr;
                }
            }
            SizeType Count()const {
                return dense_.size();
            }
            bool Test(ValueType i) {
                return GetSparseValue(i) != -1;
            }
            bool IsEmpty()const {
                return dense_.empty();
            }
        private:
            struct SparsePage
            {
                static constexpr auto PAGE_SIZE = 4096u;
                static constexpr auto PAGE_COUNT = 128u;
                ValueType    data_[PAGE_SIZE];
                BitSet<PAGE_SIZE>    flags_;
            };
            SparsePage* CreateSparsePage() {
                SparsePage* page = page_alloc_.Alloc();
                std::fill_n(page->data_, SparsePage::PAGE_SIZE, -1);
            }
            void ReleaseSparsePage(SparsePage*& page, uint32_t index) {
                page.flags_.reset(index)
                if (page.flags_.none()) {
                    page_alloc_.Dealloc(page);
                    page = nullptr;
                }
            }
            void SetSparseValue(ValueType pos, ValueType value) {
                const auto page_index = std::ceil(pos / SparsePage::PAGE_SIZE);
                if (sparse_pages_[page_index] == nullptr) {
                    sparse_pages_[page_index] = CreateSparsePage();
                }
                const auto inner_index = pos % SparsePage::PAGE_SIZE;
                sparse_pages_[page_index]->data_[inner_index] = value;
                sparse_pages_[page_index]->flags_.set(inner_index); 
            }
            void UnSetSparseValue(ValueType pos) {
                const auto page_index = std::ceil(pos / SparsePage::PAGE_SIZE);
                if (sparse_pages_[page_index] != nullptr) {
                    const auto inner_index = pos % SparsePage::PAGE_SIZE;
                    sparse_pages_[page_index]->data_[inner_index] = -1;
                    ReleaseSparsePage(sparse_pages_[page_index], innder_index);
                }
            }
            void GetSparseValue(ValueType pos) const {
                const auto page_index = std::ceil(pos / SparsePage::PAGE_SIZE);
                if (sparse_pages_[page_index] != nullptr) {
                    const auto inner_index = pos % SparsePage::PAGE_SIZE;
                    return sparse_pages_[page_index]->data_[inner_index];
                }
                return -1;
            }
        private:
            using DenseAllocatorType = std::allocator_traits<Allocator>::template rebind_alloc<ValueType>;
            using PageAllocatorType = std::allocator_traits<Allocator>::template rebind_alloc<SparsePage>;
            Vector<ValueType, DenseAllocatorType>   dense_;
            SmallVector<SparsePage*, SparsePage::PAGE_COUNT>    sparse_pages_;
            PageAllocatorType*  page_allocator_{nullptr};
        };


        //archetype storage with fixed-size and SOA layout
        class ArcheTypeChunk
        {
            //chunk index in archetypebase chunk vector
            size_type chunk_index_{ 0u };
            size_type entity_count_{ 0u };
            size_type capacity_{ ~0u };
            size_type num_component_{ 0u };
            ArcheTypeChunk* prev_{ nullptr };
            ArcheTypeChunk* next_{ nullptr };
            //when push to free list set this to archetype's free list head
            ArcheTypeChunk* free_next_{ nullptr };

            //component storage(right after the header or aligned)
            uint8_t** component_storage_{ nullptr };
            //optional entity ids(helps with swap-remove)
            uint32_t* entity_ids_{ nullptr };
            uint8_t* component_strides_{ nullptr }; //not store here, just pointer to archetype strides

        public:
            friend class ArcheTypeTable;
            static constexpr size_type CHUNK_SIZE = 16u * 1024u; //default chunk size 16K
            static constexpr size_type MIN_CHUNK_CAPACITY = 16u; //below this capacity, will do recyle work
            static constexpr size_type MAX_CHUNK_CAPACITY = 2048u; //
        public:
            bool IsEmpty() const noexcept {
                return entity_count_ == 0u;
            }

            bool IsFull() const noexcept {
                return entity_count_ == capacity_;
            }

            bool IsInFreeList() const noexcept {
                return free_next_ != nullptr; //so allocate a chunk must set free_next_ to nullptr
            }

            uint32_t GetEntityCount()const {
                return entity_count_;
            }

            [[nodiscard]] void* GetComponent(size_type component_index, uint32_t slot) const noexcept
            {
                assert(component_index < num_components_ && "Invalid component index");
                assert(slot < entity_count_ && "Slot out of range");
                return component_storage_[component_index] + (slot * component_strides_[component_index]);
            }

            void WriteComponenet(uint32_t slot, size_type component_index, const void* data, size_type data_size) noexcept
            {
                assert(component_index < num_components_ && "Invalid component index");
                assert(slot < entity_count_ && "Slot out of range");
                assert(data_size == component_strides_[component_index] && "Data size mismatch");
                std::memcpy(GetComponent(component_index, slot), data, data_size);
            }

            void RemoveComponent(size_type component_index, uint32_t slot) noexcept
            {
                assert(component_index < num_components_ && "Invalid component index");
                assert(slot < entity_count_ && "Slot out of range");
                //todo swap with last slot to keep dense
            }

            uint32_t SwapAndRemove(uint32_t slot);


        };

        using ArcheChunkList = Utils::IntrusiveLinkedList<Utils::DefaultIntrusiveListTraits<ArcheTypeChunk>>;

        struct ArcheTypeSignature
        {
            SmallVector<ComponentID> sorted_component_types_;

            ArcheTypeSignature() = default;
            explicit ArcheTypeSignature(const Vector<ComponentID>& component_ids) :sorted_component_types_(component_ids.begin(), component_ids.end()) {
            }
            explicit ArcheTypeSignature(const ArcheTypeSignature& other) :sorted_component_types_(other.sorted_component_types_) {
            }

            const bool HasComponent(ComponentID id) const {
                //fixme if signature components numbder less than 100, use linear search (find)
                return eastl::binary_search(sorted_component_types_.begin(), sorted_component_types_.end(), id);
            }

            void RegisterComponent(ComponentID id) {
                sorted_component_types_.emplace_back(id);
            }

            void UnRegisterComponent(ComponentID id) {
                if (!HasComponent(id)) UNLIKELY {
                    return;
                }
                auto iter = eastl::binary_search(sorted_component_types_.begin(), sorted_component_types_.end(), id);
                if (iter != sorted_component_types_.end()) {
                    sorted_component_types_.erase(iter);
                }
            }

            //finalize signaure, sort all component ids
            void Finalize() {
                eastl::sort(sorted_component_types_.begin(), sorted_component_types_.end());
            }

            //check whether this archetype is a superset of other's 
            bool IsContain(const ArcheTypeSignature& other) const
            {
                if (sorted_component_types_.size() < other.sorted_component_types_.size())
                {
                    return false;
                }

                for (auto other_iter = other.sorted_component_types_, cbegin(); other_iter != other.sorted_component_types_.cend(); ++other_iter)
                {
                    if (sorted_component_types_.find(*other_iter) == sorted_component_types_.end())
                    {
                        return false;
                    }
                }
                return true;
            }

            ArcheTypeSignature& Combine(ArcheTypeSignature&& other)
            {
                if (this == &other) {
                    return *this;
                }

                for (auto id : other.sorted_component_types_)
                {
                    //find first position >= id
                    auto it = eastl::lower_bound(sorted_component_types_.begin(), sorted_component_types_.end(), id);

                    // Already exists ¡ú skip
                    if (it != sorted_component_types_.end() && *it == id)
                    {
                        continue;
                    }

                    //insert before it (keeps sorted order)
                    sorted_component_types_.insert(it, id);
                }

                // Clear moved-from object (good practice + helps debugging)
                other.sorted_component_types_.clear();
            
                return *this;
            }

            const size_type GetNumComponents() const {
                return sorted_component_types_.size();
            }

            //const size_type GetComponentIndex(ComponentID id) const {
            //    auto iter = eastl::lower_bound(sorted_component_types_.begin(), sorted_component_types_.end(), id);
            //    if (iter != sorted_component_types_.end() && *iter == id) {
            //        return std::distance(sorted_component_types_.begin(), iter);
            //    }
            //    return ~0u;
            //}

        };

        class ArcheTypeGraph;

        class ArcheTypeTable
        {
        public:
            ArcheTypeTable(EntityManager& entity_manager, ArcheTypeGraph& archetype_graph, const ArcheTypeSignature& signature) :entity_manager_(entity_manager), archetype_graph_(archetype_graph), signature_(signature) {

            }
            ArcheTypeTable(ArcheTypeTable&& other);
            ArcheTypeTable& operator=(ArcheTypeTable&& other);

            virtual ~ArcheTypeTable() = default;

            //finalize and build&cache chunk relaated components info
            void Finalize(const ComponentTypeRegistry& registry);
            void Release();
            void Tick(Ticks current_tick) {
                last_tick_ = current_tick;
            }
            const ArcheTypeSignature& GetSignature() const {
                return signature_;
            }
            const size_type GetEntityCount() const {
                return entity_count_;
            }
            const size_type GetPerEntityBytes() const {
                return per_entity_bytes_;
            }
            ArcheTypeChunk* GetChunk(size_type index) {
                assert(index < chunks_.size() && "Chunk index out of range");
                return chunks_[index];
            }
            const size_type GetChunkCount() const {
                return chunks_.size();
            }
            
            //helper function for query
            bool HasComponent(ComponentID id) const {
                return signature_.HasComponent(id);
            }
            bool HasAll(const Vector<ComponentID>& ids) const;
            bool HasAny(const Vector<ComponentID>& ids) const;
            bool HasNone(const Vector<ComponentID>& ids) const;

            bool AddComponent(Entity entity, ComponentID component_id, const void* data = nullptr, size_type data_size = 0u);
            bool RemoveComponent(Entity entity, ComponentID component_id);
            
            /*chunk helper function*/
            template<class Func>
            void ForEachChunk(Func&& func) {
                for (auto* chunk : chunks_) {
                    func(*chunk);
                }
            }

            template<class Func>
            void ForEachChunk(Func&& func) const {
                for (const auto* chunk : chunks_) {
                    func(*chunk);
                }
            }

        protected:
            //allocate
            template<class Allocator>
            ArcheTypeChunk* AllocateChunk(Allocator& allocator) {
                if (free_head_ == nullptr) {
                    void* mem = allocator.allocate(ArcheTypeChunk::CHUNK_SIZE);
                    if (!mem) return nullptr;
                    auto* chunk = ::new(mem)ArchetypeChunk;
                    return chunk;
                }

                auto* chunk = free_head_;
                free_head_ = std::exchange(chunk->free_next_, nullptr);
                --free_count_;
                return chunk;
            }

            void FreeChunk(ArcheTypeChunk* chunk) noexcept
            {
                chunk->free_next_ = eastl::exchange(chunk, free_head_);
                ++free_count_;
            }

        protected:

            void LinkNonFull(ArcheTypeChunk& chunk)
            {
                non_full_chunks_.push_back(&chunk);
            }

            void UnLinkNonFull(ArcheTypeChunk& chunk)
            {
                non_full_chunks_.remove(&chunk);
            }

            //main stream design, said better than search in signature each time
            size_type GetComponentIndex(ComponentID id) const {
                auto iter = id_to_index_.find(id);
                if (iter != id_to_index_.end()) {
                    return iter->second;
                }
                return ~0u;
            }

            void BuildChunkLayout(ArcheTypeChunk& chunk) noexcept;
            //rough estimate chunk header size
            size_type EstimateChunkHeaderSize(size_type capacity = ArcheTypeChunk::MAX_CHUNK_CAPACITY)const;
            //rought estimate per-entity total size(not care alignment)
            size_type EstimatePerEntitySize(const ComponentTypeRegistry& registry)const;
            //rough estimate chunk entity capacity
            size_type EstimateMaxCapacity(const ComponentTypeRegistry& registry) const;
            bool TryFitWithCapacity(const ComponentTypeRegistry& registry, size_type capacity, size_type& mem_size);
            bool TryAddEntity(Entity new_entity, uint32_t& out_chunk_index, uint32_t& out_slot);
            void RemoveEntity(uint32_t chunk_index, uint32_t slot);
            bool MoveEntity(Entity entity, ArcheTypeTable& target_archetype);
        protected:
            //sorted list of component for hashing 
            ArcheTypeSignature signature_;
            //cached archetype components once after creation
            //re-used by chunk each time build chunk layout
            Vector<size_type> component_sizes_;
            Vector<size_type> component_alignments_;
            Vector<size_type> component_offsets_; //from storage start

            //component id to index 
            HashMap<ComponentID, size_type> id_to_index_;

            //entity manager & archetype graph reference for add/remove component
            EntityManager& entity_manager_;
            ArcheTypeGraph& archetype_graph_;

#ifdef ARCHETYPE_BLOOM_FILTER_ENABLED
            Utils::BloomFilter<ARCHETYPE_BLOOM_EXPECTN> bloom_filter;
#endif

            //collect full chunks to vector for index
            Vector<ArcheTypeChunk*> chunks_;
            ArcheChunkList non_full_chunks_;
            ArcheTypeChunk* free_head_ = nullptr;
            Vector<Entity, Allocator> entities_;
            size_type entity_count_{ 0u };
            uint32_t free_chunk_count_{ 0u };
            size_type per_entity_bytes_{ 0u };
            size_type max_chunk_capacity_{ 0u };
            mutable Ticks last_tick_{ 0u };
        };

        class ArcheTypeGraph final
        {
            HashMap<ArcheTypeTable*, HashMap<ComponentID, ArcheTypeTable*>> add_edges_;
            HashMap<ArcheTypeTable*, HashMap<ComponentID, ArcheTypeTable*>> remove_edges_;
            HashMap<ArcheTypeSignature, ArcheTypeTable> signature_to_archetype_;
        public:
            ArcheTypeGraph() = default;
            template<class Component, class ...OtherComponent>
            ArcheTypeTable* GetOrCreateArcheType() {
                static_assert(sizeof...(OtherComponents) + 1 >= 1, "requires at least one component type");
                ArcheTypeSignature signature;

                signagure.Add<Component>();
                (signature.Add<OtherComponent>(), ...);
                signature.Finalize();
                return GetOrCreateArcheType(signature);
            }

            ArcheTypeTable* GetOrCreateArcheType(const ArcheTypeSignature& signature) {
                auto iter = signature_to_archetype_.find(signature);
                if (iter != signature_to_archetype_.end()) {
                    return &iter->second;
                }
                
                auto& archetype = signature_to_archetype_[signature];
                archetype.Finalize();
                archetype = { signature };
                return &archetype;
            }
            
            [[nodiscard]] ArcheTypeTable* GetOrCreateAddTarget(const ArcheTypeTable* src, ComponentID id) {
                auto iter = add_edges_[src].find(id);
                if (iter != add_edges_[src].end()) {
                    auto edge_iter = iter->second.find(id);
                    if (edge_iter != iter->second.end()) {
                        return edge_iter->second;
                    }
                }

                ArcheTypeSignature target_signature(src->GetSignature());
                target_signature.RegisterComponent(id);
                target_signature.Finalize();
                auto* target = GetOrCreateArcheType(target_signature);
                add_edges_[src][id] = target;
                return target;
            }

            [[nodiscard]] ArcheTypeTable* GetOrCreateRemoveTarget(const ArcheTypeTable* src, ComponentID id) {
                auto iter = remove_edges_[src].find(id);
                if (iter != remove_edges_[src].end()) {
                    auto edge_iter = iter->second.find(id);
                    if (edge_iter != iter->second.end()) {
                        return edge_iter->second;
                    }
                }
                ArcheTypeSignature target_signature(src->GetSignature());
                target_signature.UnRegisterComponent(id);
                target_signature.Finalize();
                auto* target = GetOrCreateArcheType(target_signature);
                remove_edges_[src][id] = target;
                return target;
            }

            void Release() {
                for(auto& [_, archetype] : signature_to_archetype_] ) {
                    archetype.Release();
                }       
            }

            ~ArcheTypeGraph()
            {
                //release all archetype
                Release();
            }
        };

        //using 16-bit to save memory
        //or u can use Ticks instead
        struct ComponentVersion
        {
            uint16_t added_at_{ 0u };
            uint16_t changed_at_{ 0u };

            bool IsAddedAfter(Ticks since) const
            {
                if (added_at_ == 0u) return false;
                uint16_t diff = added_at_ - static_cast<uint16_t>(since);
                return diff < 0x8000;
            }

            bool IsChangedAfter(Ticks since) const
            {
                if (changed_at_ == 0u) return false;
                uint16_t diff = changed_at_ - static_cast<uint16_t>(since);
                return diff < 0x8000;
            }

            void MarkAdded(Ticks Ticks) {
                added_at_ = static_cast<uint16_t>(Ticks); changed_at_ = added_at_;
            }
            
            void MarkChanged(Ticks Ticks) {
                changed_at_ = static_cast<uint16_t>(Ticks);
            }

        };

        class SparseComponentRepoBase
        {
        public:
            virtual ~SparseComponentRepoBase() = default;
        };

        template<typename Component, typename Allocator>
        class SparseComponentRepo : public SparseComponentRepoBase
        {
        public:
            using Type = Component;
            using ThisType = SparseComponentRepo<Component, Allocator>;
            constexpr bool has_dense_data = !std::is_base_of_v<NoDenseDataPayload, Component>; //todo
            explicit SparseComponentRepo(Allocator& alloc) : sparse_set_(alloc), component_data_(alloc){

            }
            void Reverse(uint32_t size) {
                component_data_.reserve(size);
            }
            template<typename...Args>
            Type& Add(Entity e) {
                const auto index = ToIndex(e);
                return Append(index, std::forward<Args&&...>(args));
            }
            void Destroy(Entity e) {
                auto index = LookUp(e);
                if (index != -1) {
                    Remove(index);
                }
                if (on_release_) {
                    on_release_(this, e);
                }
            }
            void Clear() {
                //todo logic error
                for (auto iter = sparse_set_.Begin(); iter != sparse_set_.End(); ++iter) {
                    Destroy(iter->);
                }
                sparse_set_.Clear();
            }
        protected:
            template<typename... Args>
            Type& Append(uint32_t index, Args&&... args) {
                if (++size_ > 0.9 * capacity_) {
                    auto new_size = 2 * capacity_; //FIXME
                    Reverse(new_size);
                }
                Type* result = component_data_ + index;
                new(result)(std::forward<Args>(args)...);
                if (on_construct_) {
                    on_contruct_(this, );
                }
                return result;
            }

            void Remove(uint32_t index)
            {
                //swap size-1 to index, fixme deal reference
                auto ptr = (component_data_ + index);
                if constexpr (ptr->) {
                    Remove();
                }
                reinterpret_cast<Type*>(ptr)->~Type();
                *(component_data_ + index) = *(component_data_ + size_ - 1);
                --size_;
            }

            void Swap(uint32_t lhs, uint32_t rhs) {
                Swap(component_data_[lhs], component_data_[rhs]);
                sparse_set_.SwapDense(lhs, rhs);
            }

            size_type ToIndex(Entity entt) const {
                return sparse_set_.
            }

            Entity ToEntity(size_type index) const {
                return sparse_set_.
            }

            bool IsContain(Entity entt) const {
                return ToIndex(entt) != -1;
            }

            Type& Element(uint32_t index)
            {
                assert(index < component_data_.size());
                return component_data_[index];
            }

            const Type& Element(uint32_t index) const
            {
                assert(index < component_data_.size());
                return component_data_[index];
            }
            auto& Elements() {
                return component_data_;
            }
            const auto& Elements()const {
                return component_data_;
            }
            /*whether the component is dirty*/
            bool IsDirty() const {
                return dirty_version_.load() != last_extract_version_;
            }
            //delegate
            auto& OnConstruct() {
                return on_construct_;
            }
            auto& OnUpdate() {
                return on_update_;
            }
            auto& OnRelease() {
                return on_release_;
            }
        private:
            std::atomic_uint32_t dirty_version_{ 0u }; //mutable any instance->dirty_version++, if not changed skip all extract work
            uint32_t last_extract_version_{ ~0u };
            SparseSet<uint32_t, Allocator> sparse_set_; 
            //ArcheType
            using ComponentAlloc = std::allocator_traits<Allocator>::template rebind_alloc<Type>;
            Vector<Type, ComponentAlloc> component_data_; //todo atomic change
            Vector<ComponentVersion, ComponentAlloc> component_version_; //component version/Ticks
            //delegate 
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_construct_;
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_update_;
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_release_;
        };

        template<typename ...T>
        class MessageQueue
        {
        public:
            //message size 128
            struct Message
            {
                enum class Flags : uint16_t
                {
                    eNone        = 0x0,
                    eRecursive    = 0x1,
                };
                uint32_t    message_type_ : 16;
                uint32_t    message_flags_ : 16;
                float        time_{ 0 };
                uint8_t        payload_[128 - sizeof(uint32_t) - sizeof(float)];
                template<typename MessageType>
                MessageType& Cast() {
                    assert(MessageType::Type = message_type_);
                    return *reinterpret_cast<const MessageType*>(this->playload_);
                }
                bool IsRecursive() const {
                    return !!(message_flags_ & static_cast<uint16_t>(Flags::eRecursive));
                }
                friend bool operator < (const Message& lhs, const Message& rhs) {
                    return lhs.time_ < rhs.time_;
                }
            };
            using MessageHandler = std::function<void(Message&)>;
        public:
            void RegisterHandler(uint32_t mess_type, MessageHandler&& handle) {
                message_handlers_[mess_type].push_back(handle);
            }
            void Dispatch(const Message& mess) {
                for (auto& h : message_handlers_[mess.message_type_]) {
                    h(mess);
                }
            }
            template<typename Payload, typename... Args>
            void Dispatch(Args&& ..args) {
                Message mess{ Payload::message_id, 0, 0 };
                new(&mess.payload_)Payload(std::forward<Args>(args)...);
                Dispatch(mess);
            }
            bool Queue(const Message& mess) {
                message_queue_.push(mess);
                return true;
            }
            template<typename Payload, typename ...Args>
            bool Queue(float time, Args&&... args) {
                Message mess{ Payload::message_id, 0, curr_time_ + time };
                return Queue(mess);
            }
            //deal messages before curr time
            void Update(float delta_time) {
                curr_time_ += delta_time;
                while (!IsEmpty() && message_queue_.top().time_ < curr_time_) {
                    auto mess = message_queue_.top();
                    message_queue_.pop();
                    Dispatch(mess);
                }
            }
            //deal one message
            void Step() {
                if (!IsEmpty()) {
                    auto mess = message_queue_.top();
                    curr_time_ = mess.time;
                    message_queue_.pop();
                    Dispatch(mess);
                }
            }
            bool IsEmpty() const {
                return message_queue_.empty();
            }
            const Message& Top()const {
                return message_queue_.top();
            }
            void Reset() {
                priority_queue_ = decltype(priority_queue_)();
                curr_time_ = 0;
            }
        protected:
            float curr_time_{ 0 };
            Map<uint32_t, SmallVector<MessageHandler>> message_handlers_;
            std::priority_queue<Message, Vector<Message>, std::less<Message>> message_queue_;
        };

        enum class EUpdatePhase : uint8_t
        {
            eFrameStart,
            ePaused, //idle mode, u have to do render/resource streaming etc
            eFrameEnd,
            ePrePhysX,
            eDuringPhysX,
            ePostPhysX,
            eNum,
        };

        enum class EQueryCacheStragegy : uint8_t
        {
            eNone,
            eAuto,      //cache cacheable terms
            aAll,       //require all tems cacheable
            eDefault,   //based on context
        };

        struct ECSSystemUpdateContext {
            float   delta_time_{ 0.f };//in seconds
            uint64_t    frame_index_{ 0u }; //frame index
            EUpdatePhase    phase_{ EUpdatePhase::eFrameStart };
        };
        
        //deal with component from entity
        struct ECSComponentCommandIR
        {
            enum class EType
            {
                eUnkown,
                eContruct,
                eUpdate,
                eRelease,
                eNum,
            };
            EType   type_{ EType::eUnkown };
            String  debug_name_;
            std::function<void(void)>  task_; //command work
        };

        /*collect component add/rm/update commands*/
        //MPSC like unity consumer command only on main thread, producers on each job system
        template<typename Allocator>
        class ComponentCommandBuffer
        {
        public:
            using CmdBufferContainer = Vector<ComponentCommandIR, std::allocator_traits<Allocator>::rebind_alloc<ECSComponentCommandIR>>;
            using ConstIterator = CmdBufferContainer::const_iterator;

            explicit ComponentCommandBuffer(Allocator& alloc, size_type int_size=1u):cmd_buffer_(int_size, alloc) {
            }
            ConstIterator CBegin() const {
                assert(std::this_thread::get_id() == xx && "current on engine main thread");  
                return cmd_buffers_.cbegin();
            }
            ConstIterator CEnd() const {
                assert(std::this_thread::get_id() == xx && "current on engine main thread");  
                return cmd_buffers_.cend();
            }
            size_type Size() const {
                assert(std::this_thread::get_id() == xx && "current on engine main thread");  
                return cmd_buffers_.size();
            }
            void Push(ECSComponentCommandIR&& cmd) { 
                Utils::SpinLock lock(write_lock_);
                cmd_buffers_.emplace_back(cmd);
            }
            Optional<ECSComponentCommandIR> Poll() {
                assert(std::this_thread::get_id() == xx && "current on engine main thread");  
                if (cmd_buffers_.empty()) {
                    return {};
                }
                else
                {
                    auto temp = std::move(cmd_buffer_.front());
                    cmd_buffer_.pop_front();
                    return temp;
                }
            }
        private:
            std::atomic_bool write_lock_;
            CmdBufferContainer cmd_buffers_;
        };

        template<typename Allocator, typename Function>
        static inline void EnqueueCommandBuffer(ComponentCommandBuffer<Allocator>& command_buffer, String& debug_name, ECSComponentCommandIR::EType type, Function&& function) {
            ECSComponentCommandIR cmd_ir{ .type_ = type, .debug_name_ = std::move(debug_name), .task_ = std::move(function) };
            command_buffer.Push(cmd_ir);
        }

        class ECSSystem
        {
        public:
            virtual void Init() = 0;
            virtual void UnInit() = 0;
            virtual void Update(ECSSystemUpdateContext& ctx) = 0;
            //default return true for all phase/all other system
            virtual bool IsPhaseUpdateBefore(const ECSSystem& other, EUpdatePhase phase)const { return IsPhaseUpdateEnabled(phase); }
            virtual bool IsPhaseUpdateEnabled(EUpdatePhase phase)const { return true; }
            virtual ~ECSSystem() = default;
        private:
            std::atomic<Ticks> ticks_counter_{ 0u }; //atomic to sync
        };

        //similar to unity system group & flecs schedule
        class ECSSystemGroup : public ECSSystem
        {
        public:
            void Init() override;
            void UnInit() override;
            void Update(ECSSystemUpdateContext& ctx) override;
            bool IsPhaseUpdateBefore(const ECSSystem& other, EUpdatePhase phase)const override;
            bool IsPhaseUpdateEnabled(EUpdatePhase phase)const override;
            template<typename Function>
            void Enumerate(Function&& func) {
                for (auto sys : sub_sys_) {
                    func(sys);
                }
            }
            void AddSubSystem(ECSSystem* sub_system);
            //todo change the interface
            void RemoveSubSystem(ECSSystem* sub_system);
            virtual ~ECSSystemGroup();
        private:
            bool IsSubSystemIncluded(ECSSystem* sub_system);
            void Sort();
        private:
            SmallVector<ECSSystem*>    sub_systems_;
        };

        using ObserverCallback = std::funcction<void(Entity)>;
        using ObserverCallbackWithData = std::function<void(Entity, void*)>;

        struct Observer
        {
            Vector<ObserverCallback> on_add_;
            Vector<ObserverCallback> on_update_;
            Vector<ObserverCallback> on_remove_;
        };

        class ObserverRegistry final
        {
            HashMap<ComponentID, Observer> observer_hooks_;   
        public:
            void ConntectAdd(ComponentID id, ObserverCallback&& callback) {
                observer_hooks_[id].on_add_.emplace_back(std::move(callback));
            }
            void ConnectUpdate(ComponentID id, ObserverCallback&& callback) {
                observer_hooks_[id].on_update_.emplace_back(std::move(callback));
            }
            void ConnectRemove(ComponentID id, ObserverCallback&& callback) {
                observer_hooks_[id].on_remove_.emplace_back(std::move(callback));
            }
            void TriggerAdd(ComponentID id, Entity e) {
                for (auto& callback : observer_hooks_[id].on_add_) {
                    callback(e);
                }
            }
            void TriggerUpdate(ComponentID id, Entity e) {
                for (auto& callback : observer_hooks_[id].on_update_) {
                    callback(e);
                }
            }
            void TriggerRemove(ComponentID id, Entity e) {
                for (auto& callback : observer_hooks_[id].on_remove_) {
                    callback(e);
                }
            }
        };

        template<template<class> class ECSAdmin, class Allocator>
        class EntityTT;

        template<template<class> class ECSAdmin, class Allocator, class... Required, class... Optional, class... Excluded>
        class QueryTT;

        template<typename Allocator=Utils::LinearAllocator<void>, typename ...T>
        class ECSAdmin : public MessageQueue<T...>
        {
        public:
            using ThisType = ECSAdmin<T...>;
            using EntityType = EntityTT<ThisType, Allocator>;
        public:
            ECSAdmin() {
                //must initial singleton entity
                singleton_entity_ = entity_manager_.Spawn();
            }

            ~ECSAdmin() {
                //release singleton components
                RemoveEntity(singleton_entity_);
                //todo other things
            }

            template<typename System, typename... Args>
                requires std::is_convertible_v<ECSSystem, System>
            System* RegisterSystem(Args&&... args) {
                auto ptr = new System(std::forward<Args>(args)...);
                systems_.insert(std::make_pair(std::type_inex(typeid(System), ptr);
                //if constexpr (has_component<System>) {
                //    RegisterComponentMessage(ptr, ,std::bool_constant<>());
                //}
                //FIXME message handler
                {
                    //auto dummy[] = { (RegisterMessageHandler<System, T...>(ptr), 0)... };
                }
                return ptr;
            }
            template<typename System>
                requires std::is_base_of_v<ECSSystem, System>
            System* GetSystem() {
                static const auto key = std::type_index(typeid(System));
                if (auto iter = systems_.find(key)) {
                    return static_cast<System*>(iter->second);
                }
                return nullptr;
            }
            template<typename Component>
            void RegisterComponent(ComponentRepo<Component>* component_repo) {
                components_data_.insert(std::make_pair(std::type_index(typeid(Component)), component_repo));
            }
            template<typename Component, typename... Args>
            void RegisterComponent(Args&&... args) {
                auto comp_repo = new ComponentRepo<Component>(std::forward<Args>(args)...);  
                RegisterComponent(comp_repo);
            }

            //singleton component should be register individualy
            template<typename Component, typename ...Args>
            void RegisterSingletonComponent(Args&&... args)
            {
                RegisterComponent(args);
                auto* info = const_cast<ComponentTypeInfo*>(component_registry_.GetComponentTypeInfo<Component>());
                info->flags |= COMPONENT_FLAG_IS_SINGTON;
            }

            template<typename ...Component>
            auto GetComponentRepos() {
                if constexpr (sizeof...(Component) == 0u) {
                    return nullptr;
                }
                else {
                    if constexpr (sizeof...(Component) == 1u) {
                        if (auto iter = components_data_.find(std::type_index(typeid(Component)))) {
                            return static_cast<ComponentRepo<Component>*>(iter->second);
                        }
                        LOG(ERROR) << "admin not has such component repo";
                        return nullptr;
                    }
                    else {
                        return std::make_tuple(GetComponentRepos<Component>...);
                    }
                }
            }
            template<typename Component, typename BinaryCompare>
            void Sort(BinaryCompare&& compare_op) {
                GetComponentRepo<Component>()->Sort(compare_op);
            }

            template<typename Component, typename ComponentReferred>
            void Sort() {
                GetComponentRepo<Component>()->SortAs(GetComponentRepo<ComponentReferred>());//todo
            }

            //new command buffer
            ECSComponentCommandBuffer<Allocator>& NewCommandBuffer() {
                command_buffers_.emplace_back(ECSComponentCommandBuffer<Allocator>(, alloc_));
                return command_buffers_.back();
            }
            //dispose command buffer
            void Dispose(ECSComponentCommandBuffer<Allocator>& command_buffer) {
                assert(std::this_thread::get_id() == xx && "current on engine main thread");
                //all component change work done in sequence
                for (auto command = command_buffer.Poll(); command.has_value(); ) {
                    switch (command->type_) {
                    case ECSComponentCommandIR::EType::eContruct:
                        break;//todo
                    }
                    std::invoke(command->task_);
                    command = command_buffer.Poll();
                }
            }

            template<typename ...Component, typename ...Exclued>
            ECSComponentQuery<Component..., Exclued...> Query() {
                auto owned_repos = GetComponentRepo<Component...>();
                if (std::apply([](auto...* repo) { return (repo!=nullptr&&...); }, owned_repos))
                {
                    auto exclueded_repos = GetComponentRepo<Exclued...>();
                    return { owned_repos, exclueded_repos };
                }
                LOG(ERROR) << "the query disired not existed";
            }

            void Clear() {
                ClearComponents();
                ClearSystems();
                for (auto& [_, group] : groups_) {
                    delete group;
                }
                groups_.clear();
            }
            template<typename ...Component>
            void ClearComponents() {
                if constexpr (sizeof...(Component) == 0u) {
                    for (auto [_, comp] : components_data_) {
                        //todo 
                        delete comp;
                    }
                    components_data_.clear();
                }
                else {
                    if constexpr (sizeof...(Component) == 1u) {
                        GetComponentRepo<Component>()->Clear();
                        components_data_.erase(std::type_index(typeid(Component)));
                    }
                    else {
                        (ClearComponents<Component>(), ...); //fold express
                    }
                }
            }
            template<typename... System>
            void ClearSystems() {
                if constexpr (sizeof...(System) == 0u) {
                    for (auto [_, sys] : systems_) {
                        delete sys;
                    }
                }
                else {
                    if constexpr (sizeof...(System) == 1u) {
                        //todo finalize system
                        systems_.erase(std::type_index(typeid(System)));
                    }
                    else {
                        (ClearSystems<System>(), ...);
                    }
                }
            }
            virtual ~ECSAdmin() {
                Clear();
            }

            template<typename Component, typename... Args>
            void AddComponent(Entity e, Args&&... args) {
                //todo add/remove archetype
            }

            void AddComponent(Entity e, ComponentID id, void* src_raw, size_type size) {
                //todo add/remove archetype
            }
            //component manager interface
            template<typename Component>
            Component GetComponent(Entity e) {
                auto component_id = component_registry_->GetComponentID<Component>();
                if (IsSparseComponent(component_id)) UNLIKELY
                {
                    const auto* repo = sparse_components_data_[component_id];
                    return repo->GetComponent(e);
                }

                auto entity_loc = entity_manager_.GetLocation(e);
                const auto* arche_type = arche_

            }
            template<typename Component>
            void RemoveComponent(Entity e) {
            }
            template<typename Component, typename... Args>
            void UpdateOrCreateComponent(Entity e,  Args&&... args) {
                auto it = component_data_.find(typeid(Component));
                assert(component_data_.end() != it);
                const auto index = it->second->LookUp(e);
                if (index != -1) {
                    new(it->second->Element(index)) Component(std::forward<Args...>(args));
                }
                else {
                    comp_iterface->Add(std::forward<Args...>(args));
                }
            }
            template<typename Component>
            bool HasComponent(Entity e) {
                auto* val = GetComponent<Component>();
                return val != nullptr;
            }

            //singleton component api
            template<typename Component, typename... Args>
            bool AddSingletonComponent(Args&&... args) {
                auto id = component_registry_.GetComponentID<Component>();
                assert(IsSingletonComponent(id, component_registry_.GetComponentTypeInfo<Component>()) && "component should be a singleton");

                if (!HasSingletonComponent<Component>()) {
                    using ComponentAllocator = std::allocator_traits<Allocator>::rebind_alloc<Component>;
                    auto* data = std::allocator_traits<ComponentAllocator>::allocate(alloc_, 1);
                    std::allocator_traits<ComponentAllocator>::construct(alloc_, data, std::forward<Args>(args)...);
                    single_data_.insert(std::make_pair(typeid(Component), data)); //todo
                    return true;
                }
                LOG(ERROR) << "already has one singleton instance";
                return false;
            }

            template<typename Component>
            Component* GetSingletonComponent() {
                assert(HasSingletonComponent<Component>());
                return GetComponent<Component>(singleton_entity_);
            }

            template<typename Component>
            bool HasSingletonComponent() const {
                return HasComponent<Component>(singleton_entity_);
            }

            template<typename Component>
            void RemoveSingletonComponent() {
                assert(HasSingltonComponent<Component>());
                RemoveComponent<Component>(singleton_entity_);
            }
        
            //check whether a component enable
            template<typename Component>
            requires std::is_base_of_v<ECSEnableComponent, Component>
            bool HasEnableComponent(Entity e) {
                if (HasComponent<Component>(e)) {
                    const auto* val = GetComponent<Component>(e);
                    return static_cast<ECSEnableComponent*>(val)->enable_;
                }
                return false;
            }

            Allocator& GetAllocator() {
                return alloc_;
            }

            Ticks GetLastChangeTick() const {
                return last_change_tick_;
            }

        private:
            DISALLOW_COPY_AND_ASSIGN(ECSAdmin);

            template<typename System, typename Message, typename C, typename... Cs>
            void RegisterComponentMessage(System* sys, , std::true_type&&) {
                ComponenentCollector<Cs...> collector;
                auto run = [&](Message& mess, ComponentInterface* comp_interface) {
                    //auto call_back = [&](Message& mess, )
                }
                for (auto n = 0; n < collector.COMP_SIZE; ++n) {
                    auto comp_iter = collector.Get<n>();
                    RegisterHandler(comp_iter, [&](Message& mess) {
                        run(mess, comp_iter);
                    });
                }
            }
            template<typename System, typename Message, typename C, typename... Cs>
            void RegisterComponentMessage(System* sys, , std::false_type&&) {
                //no operation
            }
            template<typename System, typename MessageType>
            void RegisterMessageHandler(System* s) {
                //check where system support messagetype
                if (constexpr(has_message<System, MessageType>)) {
                    auto sys_handle = [&](MessageType& mess) {
                        //system update bind component 
                        for (auto [component_id, comp_data] : component_data_) {
                            s->Update(mess. comp_data);
                        }
                    };

                    RegisterHandler(, sys_handle);
                }
            }

            //entity related function
            Entity NewEntity() {
                auto entity = entity_manager_.Spawn();
                return entity;
            }

            void RemoveEntity(Entity e) {
                for (auto [_, comp_repo] : components_data_) {
                    comp_repo->Remove(e); //todo slow
                }
                entity_manager_.Release(e);
            }
            //copies an existing entity and creates a new entity from that copy
            Entity Clone(Entity prefab) {
                auto new_entity = NewEntity();
                auto prefab_loc = entity_manager_->GetLocation(prefab);
                const auto* arche_type = preface_loc.arche_type_;
                const auto* arche_chunk = arche_type->GetChunk(prefab_loc.chunk_index_);
                const auto& signaure = arche_type->GetSignature();

                //dense component copy
                size_type component_index{ 0u };
                for (auto component_id : signaure.sorted_component_types_)
                {
                    auto* component_data = arche_chunk->GetComponent(component_index, prefab_loc.slot_);
                    const auto component_size = arche_type->GetComp
                    AddComponent(new_entity, comp_data); //todo perfect forward     
                }

                LIKELY
                if (!TestEntitySparse(prfab_loc)){
                    return;
                }
                //sparse component copy
                for (auto bit = 0u; bit < sizeof(prefab_loc.sparse_mask_) * 8u; ++bit) {
                    if (prefab_loc.sparse_mask_ & (1u << bit)) {
                        const auto* comp_repo = sparse_components_data_[bit];
                        auto& comp_data = comp_repo->GetComponent(prefab);
                        AddComponent(new_entity, comp_data); //todo perfect forward
                    }
                }

            }
            //move entity from other world to current world
            template<class UAllocator, class ...UComponent>
            bool CloneExternalRetainID(Entity prefab, ECSAdmin<UAllocator>& external) {
                if (entity_manager_.Insert(prefab))
                {
                    ( ( external.HasComponent<UComponent>(prefab)
                        ? ( (AddComponent<UComponent>(prefab, external.GetComponent<UComponent>(prefab))), 0u )
                        : 0u ), ... );
                    return true;
                }

                //clone failed
                return false;
            }
            template<class UAllocator, class ...UComponent> 
            void CloneExternal(Entity prefab, ECSAdmin<UAllocator>& external) {
                auto new_entity = NewEntity();
                ( ( external.HasComponent<UComponent>(prefab)
                    ? ( (AddComponent<UComponent>(new_entity, external.GetComponent<UComponent>(prefab))), 0u )
                    : 0u ), ... );
            }
        private:
            Allocator alloc_;
            EntityManager entity_manager_;
            ComponentTypeRegistry component_registry_;
            ArcheTypeGraph archetype_graph_;
            ObserverRegistry observer_registry_;

            //entity that manage all singleton component 
            Entity singleton_entity_; 

            //we just use component id to index sparse components 
            SparseComponentRepoBase* sparse_components_data_[MAX_SPARSE_COMPONENT_ID + 1u];

            template <typename... Components>
            struct ComponenentCollector
            {
                static constexpr const uint32_t COMP_SIZE = sizeof...(Components);
                ComponentCollector(ThisType& world) {
                    Fill<0, Components...>(world);
                }
                ComponentRepoBase* components_data_[COMP_SIZE];
                template<uint32_t index>
                ComponentRepoBase* Get() { return components_data_[index]; }
                template <uint32_t index>
                void Fill(Map<std::type_index, ComponentRepoBase*>& components) {
                    return;
                }
                template <uint32_t index, typename T, typename... Ts>
                void Fill(Map<std::type_index, ComponentRepoBase*>& components) {
                    components_data_[index] = components[std::type_index(typeid(T))];
                    Fill<index + 1, Ts...>(components);
                }
            };

            //according to entt, type_index/type_info.hash_code both not unique
            Map<std::type_index, ECSSystem*> systems_;
            //ecs group define 
            Map<std::type_index, ECSComponentGroupBase*> groups_;
            //command buffers
            SmallVector<ECSComponentCommandBuffer<Allocator>> command_buffers_;

            //world Ticks
            std::atomic<Ticks> change_tick_{ 1u };
            Ticks last_change_tick_{ 0u };
        };

        //entity warper, it used to do entity related operation like add/remove component, get component etc
        template<template<class> class ECSAdmin, class Allocator>
        class EntityTT
        {
            using World = ECSAdmin<Allocator>;
            Entity entity_;
            World& world_;
        public:
            
            explicit EntityTT(Entity entt, World& world):entity_(entt), world_(world){
            }

            template<class Component, class ...Args>
            EntityTT& Add(Args&& ...args) {
                world_.AddComponent<Component>(entity_, std::forward<Args>(args)...);
                return *this;
            }

            template<class Component>
            EntityTT& Remove() {
                world_.RemoveComponent<Component>(entity_);
                return *this;
            }

            template<class Component>
            bool Has() {
                return world_.HasComponent<Component>(entity_);
            }

            template<class Component>
            Component& Get() {
                return world_.GetComponent<Component>(entity_);
            }

            //todo other api like get component, update component, etc
        };


        template<template<class> class ECSAdmin, class Allocator, class... Required, class... Optional, class... Excluded>
        class SparseQueryIterator final
        {
            using QueryType = QueryTT<ECSAdmin, Allocator, Required..., Optional..., Excluded...>;

        public:
            struct Element
            {
                Tuple<Optional*...> optional_sparse_; //nullptr if missing
                Entity entity_{ Entity::Null };
            };
            using value_type = Element;
            using pointer = value_type*;
            using reference = value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;
        protected:
            QueryType* query_{ nullptr };
            Element current_data_;
            
            static constexpr ComponentID main_sparse_id = 0u; //maybe change to template argument
            static_assert(main_sparse_id < sizeof...(Optional), "main sparse component id should less than optional component size");

            using MainType = std::tuple_element_t<main_sparse_id, Tuple<Optional...>>;
            typename SparseSet<MainType, Allocator>::Iterator sparse_it_;
            typename SparseSet<MainType, Allocator>::Iterator sparse_end_it_;
            bool is_end_{ false };
        public:
            SparseQueryIterator() = default; //->end iterator
            explicit SparseQueryIterator(QueryType& query, bool end = false) :query_(&query), is_end_(end) {
                if (is_end || query.opt_ids_.empty()) {
                    return;
                }

                //initial sparset component iterator
                auto* main_sparse_repo = query.world_.GetComponentRepo(main_sparse_id);
                if(!main_sparse_repo) {
                    LOG(ERROR) << "the main sparse component repo is null";
                    is_end_ = true;
                    return;
                }

                sparse_it_ = main_sparse_repo->begin();
                sparse_end_it_ = main_sparse_repo->end();

                is_end_ = (sparse_it_ == sparse_end_);
            }
            auto& operator++() {
                if (!is_end_) {
                    ++sparse_it_;
                    is_end_ = (sparse_it_ == sparse_end_);
                }
                return *this;
            }
            auto operator++(int) {
                auto temp = *this;
                ++(*this);
                return temp;
            }
            reference operator*() const {
                UpdateCurrentElement();
                return current_data_;
            }
            pointer operator->() const {
                UpdateCurrentElement();
                return &current_data_;
            }
            bool operator==(const SparseQueryIterator& rhs) const {
                return is_end == rhs.is_end && sparse_it == rhs.sparse_it;
            }

            bool operator!=(const SparseQueryIterator& rhs) const {
                return !(*this == rhs);
            }
        private:
            void UpdateCurrentElement() const {
                if (is_end) {
                    current_data_ = {};
                    return;
                }

                //get entity
                auto entity = sparse_it_->entity_;
                current_data_.entity_ = entity;
                //fill sparse optional
                size_type index = 0u;
                ((std::get<Optional*>(current_data_.optional_sparse_) = query->world_.HasComponent<Optional>(entity) ? query->world_.GetComponent<Optional>(entity) : nullptr, ++index), ...);  
            }

        };

        template<template<class> class ECSAdmin, class Allocator, class... Required, class... Optional, class... Excluded>
        class QueryIterator final
        {
            using QueryType = QueryTT<ECSAdmin, Allocator, Required..., Optional..., Excluded...>;

        public:
            struct Element
            {
                Tuple<Required*...> required_;
                Tuple<Optional*...> optional_dense_;
                Tuple<Optional*...> optional_sparse_; //nullptr if missing
            };
            using value_type = Element;
            using pointer = value_type*;
            using reference = value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;
        protected:
            QueryType* query_{ nullptr };
            size_type arch_index_{ 0u };
            size_type chunk_index_{ 0u };
            size_type slot_index_{ 0u };

            mutable Element current_element_;
            bool is_end_{ false };
        public:
            QueryIterator() = default; //->end iterator
            explicit QueryIterator(QueryType& query, bool end = false) :query_(&query), is_end_(end || query...) {
            }
            auto& operator++()
            {
                if (!is_end) {
                    Step();
                }
                return *this;
            }
            auto operator++(int)
            {
                auto temp = *this;
                ++(*this);
                return temp;
            }
            reference operator*() const
            {
                UpdateCurrentElement();
                return current_element_;
            }
            pointer operator->() const
            {
                UpdateCurrentElement();
                return &current_element_;
            }
            bool operator==(const QueryIterator& other) const {
                if (is_end_ && other.is_end_) {
                    return true;
                }
                if(is_end_ || other.is_end_) {
                    return false;
                }
                return &query_ == &other.query_ && arch_index_ == other.arch_index_ && chunk_index_ == other.chunk_index_ && slot_index_ == other.slot_index_;
            }
            bool operator!=(const QueryIterator& other) const {
                return !(*this == other);
            }
        private:
            void Reset() {
                arch_index_ = 0u;
                chunk_index_ = 0u;
                slot_index_ = 0u;
                is_end_ = false;

                Step();
            }
            void Step() {
                while (arch_index_ < query->matched_archetypes_.size()) {
                    auto* arch = query->matched_archetypes_[arch_index_];
                    if (chunk_index_ >= arch->chunks_.size()) {
                        ++arch_index;
                        chunk_index_ = 0u;
                        slot_index_ = 0u;
                        continue;
                    }

                    auto* chunk = arch->chunks_[chunk_index_];
                    if (slot_index_ >= chunk->entity_count_)
                    {
                        ++chunk_index_;
                        slot_index_ = 0u;
                        continue;
                    }
                    ++slot_index;
                    return;
                }
                is_end_ = true;
            }
            void UpdateCurrentElement() {
                current_element_ = {};
                if (is_end) {
                    return;
                }
                auto* arch = query_->matched_archetypes_[arch_index_];
                auto* chunk = arch->chunks_[chunk_index_];
                const current_slot = static_cast<uint32_t>(slot_index_ - 1u); //slot index already step forward in Step function

                auto entity = chunk->GetEntity(current_slot);

                size_type index = 0u;
                ((std::get<Required*>(current_element_.required_) = chunk->template GetComponenet<Required>(current_slot), ++index), ...);

                //fill sparse optional
                index = 0u;
                ((std::get<Optional*>(current_element_.optional_) = query->world_., ...);
            }
        };

        template<template<class> class ECSAdmin, class Allocator, class... Required, class... Optional, class... Excluded>
        class QueryTT
        {
            //because all sparseset component in Optional, so just test whether Required is empty 
            static constexpr auto IS_PURE_SPARSE = sizeof...(Required) == 0u;

            using World = ECSAdmin<Allocator>;
            using RequireTuple = std::tuple<Required...>;
            using OptionalTuple = std::tuple<Optional...>;
            using ExcluededTuple = std::tuple <Excluded...>;
            using Iterator = std::conditional_t<IS_PURE_SPARSE, QueryIterator<ECSAdmin, Allocator, Required..., Optional..., Excluded...>,
                                                        SparseQueryIterator<>>;

            const World& world_;
            Utils::BloomFilter<QUEY_BLOOM_EXPECTN> bloom_filter;

            SmallVector<ComponentID> req_ids_;
            SmallVector<ComponentID> opt_ids_; 
            SmallVector<ComponentID> excl_ids_; 

            Vector<ArcheTypeTable*> matched_archetypes_;
            Ticks last_change_tick_;

            uint8_t cached_ : 1;
            uint8_t dirty_ : 1;
        public:
            explicit QueryTT(World& world):world_(world), cached_(0), dirty_(0) {

            }
            QueryTT& Finalize() {
                req_ids_ = { world_->template GetComponentID<Required>()... };
                opt_ids_ = { world_->template GetComponentID<Required>()... };
                excl_ids_ = { world_->template GetComponentID<Required>()... };

                //pre-sort ids for match
                eastl::sort(req_ids_.begin(), req_ids_.end());
                eastl::sort(opt_ids_.begin(), opt_ids_.end());
                eastl::sort(excl_ids_.begin(), excl_ids_.end());

                //build bloom filter
                bloom_filter_.Clear();
                for(const auto req_id : req_ids_){
                    bloom_filter.Insert(req_id);
                }

                Cache();
                last_change_tick_ = world.GetLastChangeTick();
                return *this;
            }
            bool IsCacheValid() const noexcept {
                return cached_ && last_change_tick_ == world_->GetChangeTicks();
            }
            void RebuildIfDirty() const noexcept {
                if (!IsCacheValid()) {
                    Cache();
                }
            }
            QueryTT& Cache() {
                if (!cached_ || dirty_) {
                    MatchArcheTypes();
                    cached_ = 1u;
                    dirty_ = 0u;
                }
                return *this;
            }
            QueryTT& UnCache()
            {
                matched_archetypes_.clear();
                cached_ = 0u;
                dirty_ = 1u;
            }
            Iterator begin() {
                return Iterator(*this, world_);
            }
            Iterator end() {
                return Iterator(*this, world_, true);
            }

            template<class Func>
            void ForEach(Func&& func) {
                for (auto it = begin(); it != end(); ++it) {
                    func(*it);
                }
            }

        protected:
            void MatchArcheTypes() {
                matched_archetypes_.clear();
                for (const auto& [_, arch] : world_.) {
                    //bloom filter only check required
                    if (!arch->bloom_filter_->MayMatch(bloom_filter))
                    {
                        continue;
                    }
                    //extract check, empty archetype will also be added in
                    //for maybe later it will have entity 
                    if (arch->HasAll(req_ids_) && arch->HasNone(excl_ids_)) {
                        matched_archetypes_.emplace_back(arch);
                    }
                }
            }
         };

    }
}