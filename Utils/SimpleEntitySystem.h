#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/Memory.h"
#include "Utils/Algorithm.h"
#include "Utils/SimpleJobSystem.h"
#include <limits>
#include <type_traits>
#include <typeindex>
#include <atomic>


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

        class ArcheTypeTable;
        class ArcheTypeChunk;
        class EntityManager;
        class ArchetypeGraph;
        class ObserverRegistry;

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
        constexpr size_type ARCHETYPE_MAX_COMPONENTS = 64u; 

        //marker tags for clean query syntax
        template<typename... Ts> struct Require {};
        template<typename... Ts> struct Optional {};
        template<typename... Ts> struct Changed {};
        template<typename... Ts> struct Added {};
        template<typename... Ts> struct Exclude {};

        //traits to unpack markers from the parameter pack
        template<typename... Markers> struct QueryTraits {
            using require_t = std::tuple<>;
            using optional_t = std::tuple<>;
            using changed_t = std::tuple<>;
            using added_t = std::tuple<>;
            using exclude_t = std::tuple<>;
        };

        template<typename... Rs, typename... Os, typename... Cs, typename... As, typename... Es>
        struct QueryTraits<Require<Rs...>, Optional<Os...>, Changed<Cs...>, Added<As...>, Exclude<Es...>> {
            using require_t = std::tuple<Rs...>;
            using optional_t = std::tuple<Os...>;
            using changed_t = std::tuple<Cs...>;
            using added_t = std::tuple<As...>;
            using exclude_t = std::tuple<Es...>;
        };

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

            struct SparsePage
            {
                static constexpr size_type PAGE_SIZE = 4096u;
                static constexpr size_type PAGE_COUNT = 128u;
                ValueType data_[PAGE_SIZE];
                BitSet<PAGE_SIZE> flags_;
                SparsePage() {
                    eastl::fill(eastl::begin(data_), eastl::end(data_), -1);
                }
            };
            
            explicit SparseSet(Allocator& alloc) : dense_(alloc), page_alloc_(alloc), sparse_page_(alloc) {
                //sparse_pages_.resize(SparsePage::PAGE_COUNT, nullptr);
            }   
            explicit SparseSet(SizeType size, Allocator& alloc) : dense_(alloc), page_alloc_(alloc), sparse_page_(alloc) {
                Resize(size);
            }
            void Resize(SizeType size) { //deprecated todo
                const auto old_size = Count();
                if (old_size > size) {
                    dense_.resize(size);
                }
                dense_.resize(size);
                const auto required_pages = Utils::CeilDiv(new_size, SparsePage::PAGE_SIZE);
                if (sparse_pages_.size() < required_pages)   {
                    sparse_pages_.resize(required_pages, nullptr);
                }
            }
            Iterator Begin() {
                return Iterator(dense_, 0u);
            }
            Iterator End() {
                return Iterator(dense_, static_cast<uint32_t>(dense_.size()));
            }
            void Swap(ThisType& rhs) {
                std::swap(dense_, rhs.dense_);
                std::swap(sparse_pages_, rhs.sparse_pages_);
                std::swap(page_alloc_ rhs.page_alloc_);
            }
            void SwapByDense(ValueType lhs, ValueType rhs) {
                const ValueType lhs_ent = dense_[lhs_idx];
                const ValueType rhs_ent = dense_[rhs_idx];

                SetSparseValue(lhs_ent, rhs_idx);
                SetSparseValue(rhs_ent, lhs_idx);
                std::swap(dense_[lhs_idx], dense_[rhs_idx]);
            }
            void Insert(ValueType i) {
                if (Test(i)) return; //avoid duplicates conflict
                dense_.push_back(i);
                SetSparseValue(i, dense_.size() - 1);
            }
            void Erase(ValueType i) {
                if (!Test(i)) return; //not exist 
                const auto dense_index = GetSparseValue(i);
                UnSetSparseValue(i);
                if (dense_index != dense_, size() - 1) {
                    const auto last_dense = dense_.back();
                    SetSparseValue(dense_back, dense_index);
                    dense_[dense_index] = last_dense;
                }
                dense_.pop_back();
            }
            void Erase(const Iterator& iter) {
                if (iter != End()) {
                    Erase(*iter);
                }
            }
            void Clear() {
                dense_.clear();
                for (auto* page : sparse_pages_) {
                    if (page) {
                        page->~SparsePage();
                        page_alloc_.deallocate(page);
                    }
                }
                sparse_pages_.clear();
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
            SparsePage* CreateSparsePage() {
                SparsePage* page = new(SparsePage) page_alloc_.allocate(1);
                return page;
            }
            void ReleaseSparsePage(SparsePage*& page, uint32_t index) {
                if (nullptr == page) return;
                page.flags_.reset(index)
                if (page.flags_.none()) {
                    page->~SparsePage();
                    page_alloc_.deallocate(page);
                    page = nullptr;
                }
            }
            void SetSparseValue(ValueType pos, ValueType value) {
                const auto page_index = Utils::CeilDiv(pos, SparsePage::PAGE_SIZE);
                if (sparse_pages_.size() < page_index) {
                    sparse_pages_.resize(page_index + 1, nullptr);
                }
                if(!sparse_pages_[page_index]) {
                    sparse_pages_[page_index] = CreateSparsePage();
                }

                const auto inner_index = pos % SparsePage::PAGE_SIZE;
                sparse_pages_[page_index]->data_[inner_index] = value;
                sparse_pages_[page_index]->flags_.set(inner_index); 
            }
            void UnSetSparseValue(ValueType pos) {
                const auto page_index = Utils::CeilDiv(pos, SparsePage::PAGE_SIZE);
                if (sparse_pages_.size() > page_index && sparse_pages_[page_index] != nullptr) {
                    const auto inner_index = pos % SparsePage::PAGE_SIZE;
                    sparse_pages_[page_index]->data_[inner_index] = static_cast<ValueType>(-1);
                    ReleaseSparsePage(sparse_pages_[page_index], innder_index);
                }
            }
            void GetSparseValue(ValueType pos) const {
                const auto page_index = Utils::CeilDiv(pos, SparsePage::PAGE_SIZE);
                if (page_index < sparse_pages_.size() &&sparse_pages_[page_index] != nullptr) {
                    const auto inner_index = pos % SparsePage::PAGE_SIZE;
                    return sparse_pages_[page_index]->data_[inner_index];
                }
                return -1;
            }
        private:
            using DenseAllocatorType = std::allocator_traits<Allocator>::template rebind_alloc<ValueType>;
            using PageAllocatorType = std::allocator_traits<Allocator>::template rebind_alloc<SparsePage>;
            Vector<ValueType, DenseAllocatorType> dense_;
            SmallVector<SparsePage*, SparsePage::PAGE_COUNT> sparse_pages_;
            PageAllocatorType&  page_allocator_{nullptr};
        };

        template<typename ElementType, template <typename> typename AllocatorType>
        typename SparseSet<ElementType, AllocatorType>::size_type
            SparseSet<ElementType, AllocatorType>::GetSparseValue(ValueType entity) const {
            const size_type page_idx = entity / SparsePage::PAGE_SIZE;
            if (page_idx >= sparse_pages_.size() || !sparse_pages_[page_idx]) {
                return static_cast<size_type>(-1);
            }

            const size_type inner_idx = entity % SparsePage::PAGE_SIZE;
            return sparse_pages_[page_idx]->data_[inner_idx];
        }


        //archetype storage with fixed-size and SOA layout
        class ArcheTypeChunk
        {
            //chunk index in archetypebase chunk vector
            size_type chunk_index_{ 0u };
            size_type entity_count_{ 0u };
            size_type capacity_{ ~0u };
            size_type num_components_{ 0u };
            ArcheTypeChunk* prev_{ nullptr };
            ArcheTypeChunk* next_{ nullptr };
            //when push to free list set this to archetype's free list head
            ArcheTypeChunk* free_next_{ nullptr };

            //component storage(right after the header or aligned)
            uint8_t** component_storage_{ nullptr };
            //optional entity ids(helps with swap-remove)
            EntityIndex* entity_ids_{ nullptr };
            uint8_t* component_strides_{ nullptr }; //not store here, just pointer to archetype strides

            //change mask
            uint64_t change_mask_[ARCHETYPE_MAX_COMPONENTS/64u]{ 0u }; //bit i means whether component i changed, for system with change filter
            uint8_t** slot_change_mask_{ nullptr }; //pointer to each component's change bit vector(one bit for one slot)

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

            //---------------------------------------------------------------------------
            //change detection logic
            //---------------------------------------------------------------------------
            bool HasComponentChanged(uint32_t component_index);
            bool HasSlotChanged(uint32_t component_index, uint32_t slot);
            void MarkChanged(uint32_t component_index, uint32_t slot);
            void ResetComponentChanges(uint32_t component_index);
            void ResetAllChanges();

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
            std::atomic<uint16_t> added_at_{ 0u };
            std::atomic<uint16_t> changed_at_{ 0u };

            bool IsAddedAfter(Ticks since) const
            {
                const uint16_t added_tick = added_at_.load(std::memory_order_relaxed);
                if (added_tick == 0u) return false; // Never added

                // Cast since to 16-bit (safe for wrap-around logic)
                const uint16_t since_16 = static_cast<uint16_t>(since);

                // Handle 16-bit unsigned wrap-around (e.g., tick 65535 ¡ú 0)
                // Diff < 0x8000 means "added_tick is more recent than since_16"
                const uint16_t diff = added_tick - since_16;
                return diff < 0x8000;
            }

            bool IsChangedAfter(Ticks since) const
            {
                const uint16_t changed_tick = changed_at_.load(std::memory_order_relaxed);
                if (changed_tick == 0u) return false; // Never changed

                const uint16_t since_16 = static_cast<uint16_t>(since);
                const uint16_t diff = changed_tick - since_16;
                return diff < 0x8000;
            }

            void MarkAdded(Ticks Ticks) {
                const uint16_t tick_16 = static_cast<uint16_t>(tick);
                added_at_.store(tick_16, std::memory_order_relaxed);
                changed_at_.store(tick_16, std::memory_order_relaxed); // Changed = added initially
            }
            
            void MarkChanged(Ticks Ticks) {
                const uint16_t new_tick = static_cast<uint16_t>(tick);
                uint16_t current_tick = changed_at_.load(std::memory_order_relaxed);

                // Use compare-exchange loop to atomically update only if new tick is newer
                // (handles wrap-around: new_tick is "newer" if (new_tick - current_tick) < 0x8000)
                while (true)
                {
                    // If current tick is already newer, exit (no update needed)
                    const uint16_t diff = new_tick - current_tick;
                    if (diff >= 0x8000) break;

                    // Try to swap current_tick with new_tick
                    if (changed_at_.compare_exchange_weak(
                        current_tick,
                        new_tick,
                        std::memory_order_relaxed,
                        std::memory_order_relaxed))
                    {
                        break; // Success: updated
                    }
                    //no-ops??
                }
            }
        };

        class SparseComponentRepoBase
        {
        public:
            virtual ~SparseComponentRepoBase() = default;
            virtual void Remove(Entity entity) = 0u;
            virtual void ResetAllChange() = 0u;
        };

        template<typename Component, typename Allocator>
        class SparseComponentRepo : public SparseComponentRepoBase
        {
        public:
            using Type = Component;
            using ThisType = SparseComponentRepo<Component, Allocator>;
            static constexpr bool has_dense_data = !eastl::is_empty_v<Component>;
            explicit SparseComponentRepo(Allocator& alloc) : sparse_set_(alloc), component_data_(alloc){

            }
            void Reserve(uint32_t size) {
                if (size <= capacity_) {
                    return;
                }
                if constexpr (has_dense_data) {
                    component_data_.reserve(size);
                    component_version_.reserve(size);
                }
                sparse_set_.Resize(size);
            }
            template<typename...Args>
            Type& Add(Entity e, Args&&... args) {
                const auto index = ToIndex(e);
                if (index != ~0u) {
                    return Element(index);
                }
                return Append(index, std::forward<Args&&...>(args));
            }

            bool IsContain(Entity e) const {
                return sparse_set_.Test(e);
            }
            void Clear() {
                if constexpr (has_dense_data) {
                    for (size_type i = 0; i < size_; ++i) {
                        reinterpret_cast<Type*>(component_data_ + i)->~Type();
                    }
                    component_data_.clear();
                    component_version_.clear();
                }
                sparse_set_.Clear();
                size_ = 0u;
                capacity_ = 0u;
            }
            size_type Size() const {
                return size_;
            }
            size_type Capacity() const {
                return capacity_;
            }

            template<typename... Args>
            void Add(Entity e, Args&&... args) {
                //resize if Reached Capacity Threshold (80% for Safety)
                if (size_ >= 0.8f * capacity_) {
                    const size_type new_cap = (capacity_ == 0) ? 16 : capacity_ * 2;
                    Reserve(new_cap);
                }

                sparse_set_.Insert(e);
                
                //construct component in-place
                if constexpr (has_dense_data) {
                    new(component_data_ + size_) Type(std::forward<Args&&...>(args)...);
                    component_version_[size_].MarkAdded(change_ticks_.load());
                }
                if (on_construct_) {
                    on_contruct_(this, e);
                }
                ++size_;
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
                return sparse_set_.GetSparseValue(entt);
            }

            Entity ToEntity(size_type index) const {
                assert(index < size_);
                return *sparse_set_.Begin() + index;
            }

            bool IsContain(Entity entt) const {
                return ToIndex(entt) != -1;
            }

            Type& Get(Entity e)
            {
                static_assert(has_dense_data && "empty component has no dense data");
                const auto index = ToIndex(e);
                assert(index < component_data_.size());
                return component_data_[index];
            }

            const Type& Get(Entity e) const
            {
                static_assert(has_dense_data && "empty component has no dense data");
                const auto index = ToIndex(e);
                assert(index < component_data_.size());
                return component_data_[index];
            }
            
            auto& GetVersion(Entity e)
            {
                static_assert(has_dense_data && "empty component has no version");
                const auto index = ToIndex(e);
                return component_version_[index];
            }

            const auto& GetVersion(Entity e) const
            {
                static_assert(has_dense_data && "empty component has no version");
                const auto index = ToIndex(e);
                return component_version_[index];
            }   

            //reset all change version
            void ResetAllChange() override
            {
                for (size_type i = 0; i < component_version_.size(); ++i) {
                    repo->component_version_[i].changed_at_.store(0, std::memory_order_relaxed);
                }
            }
            
            /*whether the component is dirty*/
            bool IsDirty() const {
                return change_ticks_.load() != last_change_version_;
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
            std::atomic<Ticks> change_ticks_;
            Ticks last_change_version_{ ~0u };
            size_type size_{ 0u };
            size_type capacity_{ 0u };
            SparseSet<uint32_t, Allocator> sparse_set_; 
            //ArcheType
            using ComponentAlloc = std::allocator_traits<Allocator>::template rebind_alloc<Type>;
            Vector<Type, ComponentAlloc> component_data_; 
            Vector<ComponentVersion, ComponentAlloc> component_version_; //component version/Ticks
            //delegate 
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_construct_;
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_update_;
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_release_;
        };

        enum class EUpdatePhase : uint8_t
        {
            ePrePhysics = 0,   // input, early logic
            ePhysics = 1,
            ePostPhysics = 2,   // animation, late logic
            ePreRender = 3,   // camera, UI prep
            eRender = 4,
            ePostRender = 5,   // effects, debug overlays

            eCount
        };

        struct SystemUpdateContext
        {
            float delta_time{ 0.0f };
            Ticks current_tick{ 0 };
            EUpdatePhase phase{ EUpdatePhase::eCount };
            bool is_parallel{ false };  // hint: safe to run in parallel?
        };

        enum class EQueryCacheStragegy : uint8_t
        {
            eNone,
            eAuto,      //cache cacheable terms
            aAll,       //require all terms cacheable
            eDefault,   //based on context
        };

        
        using CommandFunc = std::function<void(void)>;

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
            EType type_{ EType::eUnkown };
            String debug_name_;
            CommandFunc task_; //command work

            ECSComponentCommandIR() = default;
            explicit ECSComponentCommandIR(EType t, String name = "", CommandFunc f = {})
                : type_(t), debug_name_(std::move(name)), task_(std::move(f))
            {
            }
            bool Valid() const { return type_ != EType::Unknown && task_; }
        };

        /*collect component add/rm/update commands*/
        //MPSC like unity consumer command only on main thread, producers on each job system
        template<typename Allocator>
        class ComponentCommandBuffer final
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
            //flush all commands(main thread)
            void Flush() {
                assert("this is in main thread"); //todo
                while (auto cmd_opt = Poll()) {
                    auto& cmd = *cmd_opt;
                    if (cmd.task_) {
                        cmd.task_();
                    }
                }
            }

            //clear in a worker thread
            void Clear() {
     
                Utils::SpinLock lock(write_lock_);
                cmd_buffer_.clear();
            }

            size_type Size() const
            {
                Utils::SpinLock(write_lock_);
                return cmd_buffers_.size();
            }

            bool IsEmpty() const {
                Utils::SpinLock(write_lock_);
                return cmd_buffers_.empty();
            }

        private:
            std::atomic_bool write_lock_; //read in main thread
            CmdBufferContainer cmd_buffers_;
        };

        template<typename Allocator, typename Function>
        static inline void EnqueueCommandBuffer(ComponentCommandBuffer<Allocator>& command_buffer, String& debug_name, ECSComponentCommandIR::EType type, Function&& function) {
            ECSComponentCommandIR cmd_ir{ .type_ = type, .debug_name_ = std::move(debug_name), .task_ = std::move(function) };
            command_buffer.Push(cmd_ir);
        }

        class System
        {
        public:
            virtual ~System() = default;

            // Lifecycle
            virtual void Init() {}
            virtual void UnInit() {}

            // Core update - can be coroutine (Coro<void>)
            virtual Coro<void> Update(SystemUpdateContext& ctx) = 0;

            // Phase control
            virtual bool IsPhaseUpdateEnabled(EUpdatePhase phase) const {
                return true;  // default: run in every phase
            }

            // Dependency: return true if *this* must run BEFORE 'other' in the given phase
            virtual bool IsPhaseUpdateBefore(const System& other, EUpdatePhase phase) const {
                return false;
            }

            virtual const char* GetName() const { return "UnnamedSystem"; }

        protected:
            std::atomic<Ticks> last_update_tick_{ 0 };
        };

        //system group, manager all inner systemes
        class SystemGroup final : public System
        {
        public:
            explicit SystemGroup(const char* name = "DefaultGroup")
                : group_name_(name ? name : "DefaultGroup")
            {
            }

            ~SystemGroup() override {
                UnInit();
            }

            void Init() override {
                for (auto* sys : sub_systems_) {
                    sys->Init();
                }
            }

            void UnInit() override {
                // Reverse order cleanup
                for (auto it = sub_systems_.rbegin(); it != sub_systems_.rend(); ++it) {
                    (*it)->UnInit();
                }
                sub_systems_.clear();
            }

            Coro<void> Update(SystemUpdateContext& ctx) override {
                if (need_sort_) {
                    Sort();
                }

                SmallVector<JobHandle> phase_jobs;

                for (auto* sys : sub_systems_) {
                    if (sys->IsPhaseUpdateEnabled(ctx.phase)) {
                        // Wrap in job for parallel if allowed
                        auto job_func = [sys, ctx]() mutable -> Coro<void> {
                            co_await sys->Update(ctx);
                            sys->ticks_counter_.store(ctx.current_tick, std::memory_order_relaxed);
                            co_return;
                            };

                        // Schedule
                        auto handle = Schedule(std::move(job_func), nullptr, 0xFFFFFFFF, true);
                        phase_jobs.push_back(std::move(handle));
                    }
                }

                // Await all in parallel
                for (auto& job : phase_jobs) {
                    co_await job;
                }

                co_return;
            }

            bool IsPhaseUpdateBefore(const System& other, EUpdatePhase phase) const override {
                for (auto* sys : sub_systems_) {
                    if (sys->IsPhaseUpdateBefore(other, phase)) {
                        return true;
                    }
                }
                return false;
            }

            bool IsPhaseUpdateEnabled(EUpdatePhase phase) const override {
                for (auto* sys : sub_systems_) {
                    if (sys->IsPhaseUpdateEnabled(phase)) {
                        return true;
                    }
                }
                return false;
            }

            const char* GetName() const override { return group_name_.c_str(); }

            void AddSubSystem(System* sub_system) {
                if (!sub_system || IsSubSystemIncluded(sub_system)) return;
                sub_systems_.push_back(sub_system);
                need_sort_ = true;
            }

            void RemoveSubSystem(System* sub_system) {
                auto it = eastl::find(sub_systems_.begin(), sub_systems_.end(), sub_system);
                if (it != sub_systems_.end()) {
                    sub_systems_.erase(it);
                    need_sort_ = true;
                }
            }

            template<typename Function>
            void Enumerate(Function&& func) {
                for (auto* sys : sub_systems_) {
                    func(sys);
                }
            }

            // Inside class SystemGroup
            void DumpDependencyGraph(const char* filename = "system_dependencies.dot") const
            {
                std::ofstream file(filename);
                if (!file.is_open()) {
                    LOG(ERROR) << "Failed to open " << filename << " for writing";
                    return;
                }

                file << "digraph SystemDependencies {\n";
                file << "  rankdir=LR;\n";               // left-to-right layout
                file << "  node [shape=box, style=filled, fillcolor=lightblue];\n";
                file << "  edge [arrowsize=1.2];\n";

                // Assign unique IDs to avoid name collisions
                HashMap<const System*, std::string> node_ids;
                int node_counter = 0;

                // Helper to get or create node ID
                auto get_node_id = [&](const System* sys) -> std::string {
                    auto it = node_ids.find(sys);
                    if (it != node_ids.end()) return it->second;

                    std::string id = "sys_" + std::to_string(node_counter++);
                    node_ids[sys] = id;

                    std::string label = sys->GetName();
                    if (dynamic_cast<const SystemGroup*>(sys)) {
                        label = "[Group] " + label;
                    }

                    file << "  " << id << " [label=\"" << label << "\"];\n";
                    return id;
                    };

                // Add edges based on dependencies (per phase)
                for (const auto* sys_a : sub_systems_) {
                    std::string id_a = get_node_id(sys_a);

                    for (const auto* sys_b : sub_systems_) {
                        if (sys_a == sys_b) continue;

                        // Check if sys_a must run before sys_b in ANY phase
                        bool depends = false;
                        std::string edge_label;
                        for (int p = 0; p < static_cast<int>(EUpdatePhase::Count); ++p) {
                            auto phase = static_cast<EUpdatePhase>(p);
                            if (sys_a->IsPhaseUpdateBefore(*sys_b, phase)) {
                                depends = true;
                                if (!edge_label.empty()) edge_label += ", ";
                                edge_label += PhaseToString(phase);
                            }
                        }

                        if (depends) {
                            std::string id_b = get_node_id(sys_b);
                            file << "  " << id_a << " -> " << id_b
                                << " [label=\"" << edge_label << "\"];\n";
                        }
                    }
                }

                // If there are sub-groups, show cluster
                for (const auto* sys : sub_systems_) {
                    if (auto* group = dynamic_cast<const SystemGroup*>(sys)) {
                        std::string cluster_id = "cluster_" + std::to_string(node_counter++);
                        file << "  subgraph " << cluster_id << " {\n";
                        file << "    label = \"" << group->GetName() << "\";\n";
                        file << "    style = filled;\n";
                        file << "    fillcolor = lightgrey;\n";

                        group->Enumerate([&](const System* sub) {
                            auto id = get_node_id(sub);
                            file << "    " << id << ";\n";
                            });

                        file << "  }\n";
                    }
                }

                file << "}\n";
                file.close();

                LOG(INFO) << "Dependency graph written to: " << filename;
                LOG(INFO) << "Open with: dot -Tpng " << filename << " -o systems.png";
            }

            // Helper for phase names
            static const char* PhaseToString(EUpdatePhase phase) {
                switch (phase) {
                case EUpdatePhase::ePrePhysics:  return "PrePhysics";
                case EUpdatePhase::ePhysics:     return "Physics";
                case EUpdatePhase::ePostPhysics: return "PostPhysics";
                case EUpdatePhase::ePreRender:   return "PreRender";
                case EUpdatePhase::eRender:      return "Render";
                case EUpdatePhase::ePostRender:  return "PostRender";
                default:                         return "Unknown";
                }
            }

        private:
            bool IsSubSystemIncluded(System* sys) const {
                return eastl::find(sub_systems_.begin(), sub_systems_.end(), sys) != sub_systems_.end();
            }

            // Topological sort for dependency ordering
            void Sort()
            {
                if (sub_systems_.size() <= 1) {
                    need_sort_ = false;
                    return;
                }

                SmallVector<System*> sorted;
                SmallVector<int> indegree(sub_systems_.size(), 0);
                HashMap<System*, SmallVector<System*>> graph;

                // Build graph
                for (size_t i = 0; i < sub_systems_.size(); ++i) {
                    auto* sys_i = sub_systems_[i];
                    for (size_t j = 0; j < sub_systems_.size(); ++j) {
                        if (i == j) continue;
                        auto* sys_j = sub_systems_[j];
                        if (sys_i->IsPhaseUpdateBefore(*sys_j, EUpdatePhase::Count)) {
                            graph[sys_i].push_back(sys_j);
                            ++indegree[j];
                        }
                    }
                }

                // Kahn's algorithm
                eastl::queue<System*> q;
                for (size_t i = 0; i < sub_systems_.size(); ++i) {
                    if (indegree[i] == 0) q.push(sub_systems_[i]);
                }

                while (!q.empty()) {
                    auto* sys = q.front(); q.pop();
                    sorted.push_back(sys);

                    auto it = graph.find(sys);
                    if (it != graph.end()) {
                        for (auto* dep : it->second) {
                            auto dep_idx = eastl::find(sub_systems_.begin(), sub_systems_.end(), dep) - sub_systems_.begin();
                            if (--indegree[dep_idx] == 0) q.push(dep);
                        }
                    }
                }

                if (sorted.size() != sub_systems_.size()) {
                    // Cycle detected - log warning or keep original order
                    LOG(WARNING) << "SystemGroup cycle detected in " << group_name_;
                }
                else {
                    sub_systems_ = std::move(sorted);
                }

                need_sort_ = false;
            }

        private:
            SmallVector<System*>    sub_systems_;
            String                  group_name_;
            bool                    need_sort_{ true };
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

        template<template<class> class World, class Allocator>
        class EntityTT;

        template<template<class> class World, class Allocator, class... Markers>
        class QueryTT;

        template<typename Allocator=Utils::LinearAllocator<void>, typename ...Policies>
        class World final
        {
        public:
            World() {
                //must initial singleton entity
                singleton_entity_ = entity_manager_.Spawn();
            }

            ~World() {
                //release singleton components
                RemoveEntity(singleton_entity_);
                Clear();
            }

            //----------------------------------------------------------------------------------
            // system mamnagement
            //---------------------------------------------------------------------------------

            template<typename SystemClass, typename... Args>
                requires std::is_convertible_v<SystemClass, System>
            System* RegisterSystem(const String& group_name, Args&&... args) {
                auto* ptr = new(allocator_->allocate(sizeof(System)))System(std::forward<Args>(args)...);
                SystemGroup* group = GetOrCreateSystemGroup(group_name);
                group.AddSubSystem(ptr);
                ptr->Init();
                return ptr;
            }

            SystemGroup& GetOrCreateSystemGroup(const String& group_name) {
                if (auto iter = system_groups_.find(group_name); iter != groups_.end()) {
                    return *iter->second;
                }
                else {
                    auto* group = new (allocator_->allocate(sizeof(SystemGroup))SystemGroup;
                    system_groups_.insert(std::make_pair(group_name, group));
                    systems_.emplace_back(group);
                    return *group;
                }
            }

            void Update(SystemUpdateContext ctx) {
                //update all systems/group
                for (auto sys : systemes_) {
                    sys->Update(ctx);
                }
            }

            //----------------------------------------------------------------------------------
            // component manger logic
            //---------------------------------------------------------------------------------
            template<typename Component>
            void RegisterComponent(EComponentStorageType storage = ) {
                component_registry_.RegisterComponent<Compoent>();
            }

            template<typename Component>
            void RegisterSparseComponent() {
                component_registry_.RegisterComponent<Compoent>();
                //allocate sparse component repo, todo use allocator
                auto* ptr = allocator_->allocate(sizeof(SparseComponentRepo<Component, Allocator>));
                auto* repo = new(ptr)SparseComponentRepo<Component, Allocator>(*allocator_);
                sparse_components_data_[std::make_pair(component_registry_.GetComponentID<Component>()] =  repo;
            }

            //singleton component should be register individualy
            template<typename Component>
            void RegisterSingletonComponent()
            {
                RegisterSparseComponent();
                auto* info = const_cast<ComponentTypeInfo*>(component_registry_.GetComponentTypeInfo<Component>());
                info->flags |= COMPONENT_FLAG_IS_SINGTON;
            }

            template<typename Component>
            ComponentID GetComponentID() const {
                return component_registry_.GetComponentID<ComponentID>();
            }

            //new command buffer
            ComponentCommandBuffer<Allocator>& NewCommandBuffer() {
                command_buffers_.emplace_back(ECSComponentCommandBuffer<Allocator>(, alloc_));
                return command_buffers_.back();
            }
            //dispose command buffer
            void Dispose(ComponentCommandBuffer<Allocator>& command_buffer) {
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
                    //
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

            void ResetChangeMasks() noexcept
            {
                for (auto& [_, arche_tpye] : ) {
                    for (size_type chunk_index = 0u; chunk_index < arche_type->GetChunkCount(); ++chunk_index) {
                        auto* chunk = arche_type->GetChunk(chunk_index);
                        chunk->ResetAllChange();
                    }
                }

                for (ComponentID id = 0; id <= MAX_SPARSE_COMPONENT_ID; ++id) {
                    auto* repo = sparse_components_data_[id];
                    if (nullptr != repo) {
                        repo->ResetAllChange();
                    }
                }
            }

            //----------------------------------------------------------------------------------
            // entity manger logc
            //----------------------------------------------------------------------------------
            Entity Spawn() {
                return entity_manager_.Spawn();
            }

            Entity Spawn(Entity prefab)
            {
                return Clone(prefab);
            }

            void Remove(Entity entity) {
                RemoveAllComponents(entity);
                entity_manager_.Release(entity);
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
                    const auto* repo = GetSparseComponentRepoImpl<Component>();
                    assert(repo->IsContain(e) && "entity does not have the component");
                    return repo->GetComponent(e);
                }

                auto entity_loc = entity_manager_.GetLocation(e);
                const auto* arche_type = entity_loc.arche_type_;
                return arche_type->GetComponent(entity, component_id);
            }

            template<typename Component>
            Component* TryGetComponent(Entity e) {
                auto component_id = component_registry_->GetComponentID<Component>();
                if (IsSparseComponent(component_id)) UNLIKELY
                {
                    const auto* repo = GetSparseComponentRepoImpl<Component>();
                    return repo->IsContain(e) ? &repo->GetComponent(e) : nullptr;
                }
                auto entity_loc = entity_manager_.GetLocation(e);
                const auto* arche_type = entity_loc.arche_type_;
                if (!arche_type->HasComponent(component_id)) {
                    return nullptr;
                }
                return arche_type->GetComponent(entity, component_id);
            }

            template<typename Component>
            void RemoveComponent(Entity e) {
                auto component_id = component_registry_->GetComponentID<Component>();
                if (IsSparseComponent(component_id)) UNLIKELY
                {
                    const auto* repo = GetSparseComponentRepoImpl<Component>();
                    repo->Remove(e);
                    return;
                }
                auto entity_loc = entity_manager_.GetLocation(e);
                auto* arche_type = archetype_graph_.GetArcheType(entity_loc.arche_type_index_);
                if (!arche_type->HasComponent(component_id)) {
                    LOG(WARNING) << "entity does not have the component";
                    return;
                }
                arche_type->RemoveComponent(entity, component_id);
            }

            void RemoveAllComponent(Entity e) {
                auto entity_loc = entity_manager_.GetLocation(e);
                auto* arche_type = archetype_graph_.GetArcheType(entity_loc.arche_type_index_);
                arche_type->RemoveAllComponent(e);

                if(entity_loc.sparse_mask_ != 0u) UNLIKELY
                {
                    for (auto bit = 0u; bit <= MAX_SPARSE_COMPONENT_ID; ++bit) {
                        const auto index = 1 << bit;
                        if (entity_loc.sparse_mask_ & index) {
                            auto* repo = sparse_components_data_[bit];
                            repo->Remove(e);
                        }
                    }
                }
            }   

            template<typename Component, typename... Args>
            void UpdateOrCreateComponent(Entity e,  Args&&... args) {
                //todo
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

            //----------------------------------------------------------------------------------
            //query operations
            //----------------------------------------------------------------------------------
            template<typename ...Markers>
            auto Query() {
                return Query<World, Required..., Optional..., Excluded...>(*this);
            }   

            void BeginFrame();
            void EndFrame();

            void OnStructureChange() { change_tick_.fetch_add(1u, std::memory_order_relaxed); }

            AllocatorType& GetAllocator() {
                return allocator_;
            }

            Ticks GetLastChangeTick() const {
                return last_change_tick_;
            }

        private:
            DISALLOW_COPY_AND_ASSIGN(World);
            
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

                if (!TestEntitySparse(prfab_loc))LIKELY{
                    return Entity::null;
                }
                //sparse component copy
                for (auto bit = 0u; bit < sizeof(prefab_loc.sparse_mask_) * 8u; ++bit) {
                    if (prefab_loc.sparse_mask_ & (1u << bit)) {
                        const auto* comp_repo = sparse_components_data_[bit];
                        auto& comp_data = comp_repo->GetComponent(prefab);
                        AddComponent(new_entity, comp_data); //todo perfect forward
                    }
                }

                return new_entity;

            }
            //move entity from other world to current world
            template<class UAllocator, class ...UComponent>
            bool CloneExternalRetainID(Entity prefab, World<UAllocator>& external) {
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
            void CloneExternal(Entity prefab, World<UAllocator>& external) {
                auto new_entity = NewEntity();
                ( ( external.HasComponent<UComponent>(prefab)
                    ? ( (AddComponent<UComponent>(new_entity, external.GetComponent<UComponent>(prefab))), 0u )
                    : 0u ), ... );
            }
            template<class Component>
            SparseComponentRepo<Component, Allocator>* GetSparseComponentRepoImpl() {
                auto component_id = component_registry_.GetComponentID<Component>();
                assert(IsSparseComponent(component_id) && "component should be a sparse component");
                return static_cast<SparseComponentRepo<Component, Allocator>*>(sparse_components_data_[component_id]);
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

            //according to entt, type_index/type_info.hash_code both not unique
            Vector<System*> systems_;
            Map<String, SystemGroup*> system_groups_;
            //command buffers
            SmallVector<ComponentCommandBuffer<Allocator>> command_buffers_;

            //world Ticks
            std::atomic<Ticks> change_tick_{ 1u };
            Ticks last_change_tick_{ 0u };
        };

        //entity warper, it used to do entity related operation like add/remove component, get component etc
        template<template<class> class World, class Allocator>
        class EntityTT
        {
            using World = World<Allocator>;
            Entity entity_{Entity::null};
            World& world_;
        public:
            
            explicit EntityTT(Entity entt, World& world):entity_(entt), world_(world){
            }

            EntityTT(EntityTT&&) = default;
            EntityTT& operator=(EntityTT&&) = default;
            
            ~EnttityTT() = default;

            Entity GetID() const {
                return entity_;
            }   

            bool IsValid() const {
                return entity_.IsValid();
            }

            bool IsAlive() const {
                return entity_.IsAlive();
            }

            /**
            * @brief Add a new component (or overwrite if exists).
            * @return *this for chaining
            */
            template<class Component, class ...Args>
            EntityTT& Add(Args&& ...args) {
                static_assert(!std::is_same_v<T, void>, "Cannot add void component");
                world_.AddComponent<Component>(entity_, std::forward<Args>(args)...);
                return *this;
            }

            /**
             * @brief Set/replace an existing component (must already exist).
             * @return *this for chaining
             */
            template<typename T, typename... Args>
            EntityTT& Set(Args&&... args)
            {
                static_assert(!std::is_same_v<T, void>, "Cannot set void component");
                world_.SetComponent<T>(entity_, std::forward<Args>(args)...);
                return *this;
            }

            /**
             * @brief Remove a component if it exists.
             * @return *this for chaining
             */
            template<class Component>
            EntityTT& Remove() {
                static_assert(!std::is_same_v<T, void>, "Cannot remove void component");
                world_.RemoveComponent<Component>(entity_);
                return *this;
            }

            template<class Component, typename ...Args>
            Component& GetOrAdd(Args&&... args) {
                if (!Has<Component>()) {
                    Add<Component>(std::forward<Args>(args)...);
                }
                return Get<Component>();
            }

            //----------------------------------------------------------------------------------
            //batch operations
            //----------------------------------------------------------------------------------
            /**
             * @brief Remove multiple components at once (chainable).
            */
            template<typename... Components>
            EntityTT& RemoveAll()
            {
                (Remove<Components>(), ...);
                return *this;
            }

            /**
             * @brief Clear all components from this entity (dense + sparse).
             */
            void RemoveAllComponents()
            {
                // Remove all dense components (iterate signature if needed)
                // For sparse: clear sparse_mask and remove from sparse_stores
                world_.RemoveAllComponents(entity_);
            }

            /**
             * \brief manual destroy the entity
             */
            void Destroy() {
                world_.RemoveEntity(entity_);
                entity_ = Entity::null;
            }

            template<class Component>
            bool Has() {
                return world_.HasComponent<Component>(entity_);
            }

            template<class Component>
            Component& Get() {
                return world_.GetComponent<Component>(entity_);
            }

            template<class Component>
            Component* TryGet() {
                return world_.GetComponent<Component>(entity_);
            }

            /**
             *\brief Get const reference to component (asserts existence).
             */
            template<typename T>
            const T& Get() const
            {
                return world_.GetComponent<T>(entity_);
            }

            /**
             *\brief Get const pointer to component (returns nullptr if missing).
             */
            template<typename T>
            const T* TryGet() const
            {
                return world_.GetComponent<T>(entity_);
            }
        protected:

            EntityTT(const EntityTT&) = delete;
            EntityTT& operator=(const EntityTT&) = delete;
        };


        template<template<class> class World, class Allocator, class... Markers>
        class SparseQueryIterator final
        {
            using QueryType = QueryTT<World, Allocator, Markers...>;

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

                bool added_ok = true;
                size_type add_idx = 0;
                for (auto add_id : query_->req_added_ids_) {
                    auto* repo = query_->world_.GetComponentRepo(add_id);
                    if (!repo) {
                        added_ok = false;
                        break;
                    }

                    auto local_idx = repo->sparse_set_.Get(entity.id_);
                    if (local_idx == SparseSet<uint32_t, Allocator>::INVALID_INDEX) {
                        added_ok = false;
                        break;
                    }

                    uint16_t added_at = repo->component_version_[local_idx].added_at_.load(std::memory_order_relaxed);
                    if (added_at < query_->last_change_tick_) {
                        added_ok = false;
                        break;
                    }
                    ++add_idx;
                }

                if (!added_ok) {
                    current_data_ = {};
                    return;
                }

                bool change_ok = true;
                size_type idx = 0u;
                for (auto ch_id : query_->req_changed_ids_) {
                    auto* repo = query_->world_.GetComponentRepo(ch_id);
                    if (!repo) {
                        change_ok = false;
                        break;
                    }

                    // Get local index in repo
                    auto local_idx = repo->sparse_set_.Get(entity.id_);
                    if (local_idx == SparseSet<uint32_t, Allocator>::INVALID_INDEX) {
                        change_ok = false;
                        break;
                    }

                    // Check changed_at_ >= query_->last_system_tick_ (or world_.change_tick_)
                    uint16_t changed_at = repo->component_version_[local_idx].changed_at_.load(std::memory_order_relaxed);
                    if (changed_at < query_->last_change_tick_) {
                        change_ok = false;
                        break;
                    }

                    ++idx;
                }

                if (!change_ok) {
                    // Skip this entity (but since it's iterator, we need to advance in ++)
                    // For simplicity, assume we filter in a separate loop if needed
                    current_data_ = {};  // or throw / skip
                    return;
                }
                //fill sparse optional
                size_type index = 0u;
                ((std::get<Optional*>(current_data_.optional_sparse_) = query->world_.HasComponent<Optional>(entity) ? query->world_.GetComponent<Optional>(entity) : nullptr, ++index), ...);  
            }

        };

        template<template<class> class World, class Allocator, class... Required, class... Optional, class... Excluded>
        class QueryIterator final
        {
            using QueryType = QueryTT<World, Allocator, Required..., Optional..., Excluded...>;
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
                while (arch_index_ < query_->matched_archetypes_.size()) {
                    auto* arch = query_->matched_archetypes_[arch_index_];
                    if (chunk_index_ >= arch->GetChunkCount()) {
                        ++arch_index_;
                        chunk_index_ = 0u;
                        continue;
                    }

                    auto* chunk = arch->GetChunk(chunk_index_);
                    if (slot_index_ >= chunk->entity_count_) {
                        ++chunk_index_;
                        slot_index_ = 0u;
                        continue;
                    }

                    uint32_t curr_slot = static_cast<uint32_t>(slot_index_);

                    // Changed<T>  
                    bool changed_ok = true;
                    size_type ch_idx = 0;
                    for (auto ch_id : query_->req_changed_ids_) {
                        size_type local_idx = arch->GetComponentIndex(ch_id);
                        if (local_idx != INVALID_INDEX) {
                            uint8_t* mask = chunk->slot_change_mask_[local_idx];
                            size_type byte_idx = curr_slot / 8;
                            size_type bit_idx = curr_slot % 8;
                            if ((mask[byte_idx] & (1 << bit_idx)) == 0) {
                                changed_ok = false;
                                break;
                            }
                        }
                        ++ch_idx;
                    }

                    // Added<T>  only sparseset repo 
                    bool added_ok = true;
                    size_type add_idx = 0;
                    for (auto add_id : query_->req_added_ids_) {
                        auto* repo = query_->world_.GetComponentRepo(add_id);
                        if (!repo) {
                            added_ok = false;
                            break;
                        }

                        // ¼ÙÉè slot ¶ÔÓ¦ entity
                        Entity entity = chunk->GetEntityAt(curr_slot);
                        auto local_idx = repo->sparse_set_.Get(entity.id_);
                        if (local_idx == SparseSet<uint32_t, Allocator>::INVALID_INDEX) {
                            added_ok = false;
                            break;
                        }

                        uint16_t added_at = repo->component_version_[local_idx].added_at_.load(std::memory_order_relaxed);
                        if (added_at < query_->last_change_tick_) {
                            added_ok = false;
                            break;
                        }
                        ++add_idx;
                    }

                    if (!changed_ok || !added_ok) {
                        ++slot_index_;
                        continue;
                    }

                    ++slot_index_;
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
                ((std::get<Optional*>(current_element_.optional_) = query->world_.GetComponent<Optional>(entity), ...);
            }
        };


        template<template<class> class World, class Allocator, class... Markers>
        class QueryTT final 
        {
            friend class QueryIterator<World, Allocator, Markers...>;
            friend class SparseQueryIterator<World, Allocator, Markers...>;

            using World = World<Allocator>;
            using Traits = QueryTraits<Markers...>;

            //extract tuples
            using RequireTuple = typename Traits::require_t;
            using OptionalTuple = typename Traits::optional_t;
            using ChangedTuple = typename Traits::changed_t;
            using AddedTuple = typename Traits::added_t;
            using ExcludeTuple = typename Traits::exclude_t;

            //because all sparseset component in Optional, so just test whether Required is empty 
            static constexpr auto IS_PURE_SPARSE = (std::tuple_size_v<RequireTuple> == 0u);

            using Iterator = std::conditional_t<IS_PURE_SPARSE, QueryIterator<World, Allocator, Required..., Optional..., Excluded...>,
                                                        SparseQueryIterator<>>;
        
            World& world_;
            Utils::BloomFilter<QUEY_BLOOM_EXPECTN> bloom_filter;

            //type component ids
            SmallVector<ComponentID> req_dense_ids_;
            SmallVector<ComponentID> req_changed_ids_;
            SmallVector<ComponentID> req_added_ids_;
            SmallVector<ComponentID> opt_ids_;
            SmallVector<ComponentID> excl_ids_;

            Vector<ArcheTypeTable*> matched_archetypes_;
            Ticks last_change_tick_;

            uint8_t cached_ : 1{0u};
            uint8_t dirty_ : 1{0u};
        public:
            explicit QueryTT(World& world):world_(world), cached_(0), dirty_(0) {
                Finalize();
            }
            QueryTT& Finalize() {
                req_dense_ids_.clear();
                req_changed_ids_.clear();
                req_added_ids_.clear();
                opt_ids_.clear();
                excl_ids_.clear();

                // Extract IDs from markers
                ExtractIds<RequireTuple>(req_dense_ids_);
                ExtractIds<ChangedTuple>(req_changed_ids_);
                ExtractIds<AddedTuple>(req_added_ids_);
                ExtractIds<OptionalTuple>(opt_ids_);
                ExtractIds<ExcludeTuple>(excl_ids_);

                //pre-sort ids for match
                eastl::sort(req_dense_ids_.begin(), req_dense_ids_.end());
                eastl::sort(excl_ids_.begin(), excl_ids_.end());

#ifdef ARCHETYPE_BLOOM_FILTER_ENABLED
                //build bloom filter
                bloom_filter_.Clear();
                for(const auto req_id : req_ids_){
                    bloom_filter.Insert(req_id);
                }
#endif
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
            template<typename... Ts>
            void ExtractIds(SmallVector<ComponentID>& dense, SmallVector<ComponentID>& changed, SmallVector<ComponentID>& added) {
                
            }

            void MatchArcheTypes() {
                matched_archetypes_.clear();
                for (const auto& [_, arch] : world_) {
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

            //helper to extract IDs from tuple of types
            template<typename Tuple>
            void ExtractIds(SmallVector<ComponentID>& ids) {
                std::apply([&](auto... ts) {
                    (ids.push_back(world_.template GetComponentID<decltype(ts)>()), ...);
                    }, Tuple{});
            }
         };

    }
}