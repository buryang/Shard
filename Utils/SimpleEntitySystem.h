#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Hash.h"
#include "Utils/Memory.h"
#include "Utils/SimpleJobSystem.h"
#include "Delegate/Delegate/MulticastDelegate.h"
#include <limits>
#include <type_traits>
#include <typeindex>

//http://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system.html
//https://docs.unity3d.com/Packages/com.unity.entities@0.17/manual/ecs_core.html
//https://www.youtube.com/watch?v=jjEsB611kxs

/* now only realize sparset ecs system, which is suitable for small number of entities
 * future work:
 * 1. archetype based ecs system for better cache performance
 * 2. hybrid sparse/archetype( archetype for simulation, while sparse for render/editor ), with two seperate ECS world
 * 3. bevy like ECS system, some component with archetye while others with sparseset
 */

//todo realize wildcard, entity as component

namespace Shard
{
    namespace Utils
    { 

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
            explicit operator bool() const {
                return generation_ > 0;
            }
            uint32_t    id_ : 20;
            uint32_t    generation_ : 12;
            //fixme https://ajmmertens.medium.com/doing-a-lot-with-a-little-ecs-identifiers-25a72bd2647 
            static const Entity Null;
        };

        class MINIT_API EntityManager
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
        public:
            Vector<VersionType>    generation_;
            List<uint32_t>    free_indices_;
        };

        /*
        * component/archetype type helper traits
        */
        using ComponentID = std::type_index;
        using ArcheTypeID = xx;

        template<class component>
        struct ComponentIDTraits final 
        {
            static constexpr ComponentID ID = std::typeid(component);
        };

        template<class ArcheType>
        struct ArcheTypeIDTraits final
        {
            static constexpr auto ID = ...;
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

            }
            Iterator End() {

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
            PageAllocatorType*  page_alloc_{nullptr};
        };

        class ArcheTypeComponentContainerBase
        {
        public:
            virtual void Resize(size_type size) = 0;
            virtual ~ArcheTypeComponentContainerBase() = default;
        };

        //individual component storage for archetype table
        template<class Component, class Allocator>
        class ArcheTypeComponentContainer final : public ArcheTypeComponentContainerBase
        {
        public:
            explicit ArcheTypeComponentContainer(Allocator& allocator) :allocator_(allocator) {
            }
            void Resize(size_type size) override {
                component_repo_.resize(size);
            }
        protected:
            Allocator& allocator_;
            Vector<Component, Allocator> component_repo_;
        };

        class ArcheTypeTableBase
        {
        public:
            ArcheTypeTableBase() = default;
            virtual ~ArcheTypeTableBase() = default;

            void Resize(size_type size) {
                for (auto& [_, container] : component_storage_) {
                    container->Resize(size);
                }
                entity_count_ = size;
            }
            void Tick(uint64_t current_tick) {
                last_tick_ = current_tick;
            }

            static uint64_t CalcArchetypeHash(const Span<std::type_index>& component_types) {
                uint64_t hash{ component_types.size() };
                for (auto& type : component_types)
                {
                    hash = (hash + 1013u) ^ (type.hash_code() + 107u) << 1u;
                }
                return hash;
            }
        protected:
            auto& ResigterComponent(std::type_index type_index, ArcheTypeComponentContainerBase* container) {
                sorted_component_types.push_back(type_index);
                component_storage_[type_index] = container;
                sorted_component_types.sort();

                //regist add/remove edge

                return *this;
            }
            ArcheTypeComponentContainerBase* GetComponentContainer(std::type_index type_index) {
                auto iter = component_storage_.find(type_index);
                if (iter != component_storage_.end()) {
                    return iter->second;
                }
                return nullptr;
            }
            //traversal edges

        protected:
            //sorted list of component for hashing 
            SmallVector<ComponentID> sorted_component_types;
            size_type entity_count_{ 0u };
            mutable uint64_t last_tick_{ 0u };
        };

        struct ArcheTypeEdge
        {
            ArcheTypeTableBase& target_archetype_;
        };

        //todo support archtype
        template<class Allocator, class ...Components>
        class ArcheTypeTable final : public ArcheTypeTableBase
        {
        public:
            explicit ArcheTypeTable(Allocator& allocator) :allocator_(allocator)
            {
                (RegisterActiveComponent<Component>(), ...);

                //generate hash code
                hash_ = CalcArchetypeHash(sorted_component_types);
            }
        protected:
            Allocator& allocator_;
            uint64_t hash_;
            Vector<Entity, Allocator> entities_;
            Map<ComponentID, ArcheTypeEdge, Allocator> change_edges_; //add/remove component(type_index), result archetype
            Map<ComponentID, ArcheTypeComponentContainerBase*, Allocator> component_storage_;
        private:
            DISALLOW_COPY_AND_ASSIGN(ArcheTypeTable);
            template<class T>
            void RegisterActiveComponent() {
                const auto type_index = std::type_index(typeid(T));
                auto* container = new ArcheTypeComponentContainer<T>(allocator_); //fixme use allocator
                RegisterComponent(type_index, container);
            }
        };

        enum class EComponentStorageType :uint8_t
        {
            eDense, //dense data component, stored in contiguous memory for better cache performance, suitable for frequently accessed component
            eSparse, //sparse data component, stored in hash map or other sparse data structure, suitable for infrequently accessed component
        };

        struct ComponentVersion
        {
            uint16_t added_at_{ 0u };
            uint16_t changed_at_{ 0u };

            bool IsAddedAfter(uint32_t since) const
            {
                if (added_at_ == 0u) return false;
                uint16_t diff = added_at_ - static_cast<uint16_t>(since);
                return diff < 0x8000;
            }

            bool IsChangedAfter(uint32_t since) const
            {
                if (changed_at_ == 0u) return false;
                uint16_t diff = changed_at_ - static_cast<uint16_t>(since);
                return diff < 0x8000;
            }

            void MarkAdded(uint32_t tick) {
                added_at_ = static_cast<uint16_t>(tick); changed_at_ = added_at_;
            }
            
            void MarkChanged(uint32_t tick) {
                changed_at_ = static_cast<uint16_t>(tick);
            }

        };

        class ComponentRepoBase
        {
        public:
            using Ptr = ComponentRepoBase*;
            virtual ~ComponentRepoBase() = default;
        };

        template<typename Component, typename Allocator>
        class ComponentRepo : public ComponentRepoBase
        {
        public:
            using Type = Component;
            using ThisType = ComponentRepo<Component, Allocator>;
            constexpr bool has_dense_data = !std::is_base_of_v<NoDenseDataPayload, Component>; //todo
            explicit ComponentRepo(Allocator& alloc) : sparse_set_(alloc), component_data_(alloc){

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

            //sort component
            template<typename BinaryCompare>
            void Sort(BinaryCompare&& compare_op) {

            }

            //sort component order as target component repo
            template<typename OtherComponent>
            void SortAs(const ComponentRepo<OtherComponent>& target) {

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

            const Vector<Entity>& GetEntities() const{

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
            Vector<ComponentVersion, ComponentAlloc> component_version_; //component version/ticks
            //delegate 
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_construct_;
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_update_;
            DelegateLib::MulticastDelegate<void(ThisType*, Entity)>    on_release_;
        };

        /*
        template<typename SharedComponent>
        class SharedComponentRepo : public ComponentRepoBase
        {
        public:
            using Type = SharedComponent;
        private:
            class Chunk
            {
            public:

            private:
                Vector<Entity> entitys_;
            };
        private:
            Type* component_data_{ nullptr };
        };
        */

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

        template <typename ...T>
        struct list {

        };
        
        template <typename T> struct ListSizeImpl;
        template <typename ...T> struct ListSizeImpl<list<T...>>
        {
            using Type = std::integral_constant<int, sizeof...(T)>;
        };
        template<typename List>
        using ListSize = typename ListSizeImpl<List>::Type;
        template <typename T> struct HeadImpl;
        template <typename T, typename... Ts> struct HeadImpl<list<T, Ts...>>
        {
            using Type = T;
        };

        template <typename List>
        using Head = typename HeadImpl<List>::Type;

        struct ECSComponent
        {
            //dummy structure
        };

        template<typename T, class...>
        struct IsTypeIncluded : std::false_type {};
        template<typename T, class... TT>
        struct IsTypeIncluded : std::disjunction<std::is_same_v<std::remove_cv_t<T>, std::remove_cv_t<U>>...> {};

        template<typename ...Component>
        struct ECSComponentList
        {
            using BaseType = std::tuple<Component...>;
            using ThisType = ECSComponentList<Component...>;
            static constexpr auto SIZE = std::tuple_size_v<BaseTyoe>;
            template<typename ...TT>
            static bool Contain() {
                if constexpr (sizeof...(TT) == 1u) {
                    return IsTypeIncluded<TT, Component...>::value;
                }
                else
                {
                    return ((Contain<TT>()&&...));
                }
            }
            template<size_type Index>
            struct IndexedType{
                using Type = std::tuple_element_t<Index, BaseType>;
            };
            constexpr ECSComponentList() = default;
        };

        //list name alias
        template<typename ...Component>
        struct ECSExcludedList final : ECSComponentList<Component...> {};
        template<typename ...Component>
        struct ECSOwnedList final : ECSComponentList<Component...>{};

        template<template<typename> class, typename>
        struct ECSBatTransformList {
            using Type = ECSComponentList<>;
        };
        
        template<template<typename> typename Other, typename ...Component>
        struct ECSBatTransformList<Other, ECSComponentList<Component...>>
        {
            using Type = ECSComponentList<Other<Component>...>;
        };

        template<typename ...Owned, typename ...Rejct> 
        struct ECSEntityDescriptor
        {
            using OwnedList = ECSOwnedList<Owned...>;
            using RejectList = ECSExcluedList<Reject...>;
        };

        template<typename ...Component>
        class ECSComponentGroupIterator final
        {
        public:
            using ThisType = ECSComponentGroupIterator<Component...>;
            using ValueType = std::forward_as_tuple<std::remove_cv_t<Component>...>;
            //iterator traits field
            using value_type = ValueType;
            using pointer = value_type*;
            using reference = value_type;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            ECSComponentGroupIterator() = default;
            ECSComponentGroupIterator(size_type iter, std::tuple<auto*...>& repos):iter_(iter), components_repos_(repos) {
                //todo
            }
            ECSComponentGroupIterator& operator++() {
                ++iter_;
                //todo 
                return *this;
            }
            ECSComponentGroupIterator operator++(int) {
                ECSComponentGroupIterator temp{ *this };
                ++(*this);
                return temp;

            }
            [[nodiscard]] auto operator*() {
                return    operator->();
            }
            [[nodiscard]] auto operator->() {
                return std::apply([entity = iter_](auto...* repo) { return std::make_tuple((repo->GetComponent(entity), ...)); }, components_repos_)); 
            }
            template<typename ...ComponentLHS, typename ...ComponentRHS>
            friend constexpr bool operator==(const ECSComponentGroupIterator<ComponentLHS...>& lhs, const ECSComponentGroupIterator<ComponentRHS...>& rhs) {
                return lhs.iter_ == rhs.iter_;
            }

            template<typename ...ComponentLHS, typename ...ComponentRHS>
            friend constexpr bool operator!=(const ECSComponentGroupIterator<ComponentLHS...>& lhs, const ECSComponentGroupIterator<ComponentRHS...>& rhs) {
                return !(lhs == rhs);
            }
        private:
            //member field
            size_type    iter_;
            ECSBatTransformList<ComponentRepo*, Component...>::Type components_repos_;
        };
        
        struct ECSComponentGroupBase
        {
        };

        template<typename, typename>
        class ECSComponentGroup : public ECSComponentGroupBase
        {

        };

        /*group/bundle component together and pointer to position by cursor*/
        template<typename ...Component, typename ...Reject>
        class ECSComponentGroup<ECSOwnedList<Component...>, ECSExcludedList<Reject...>>
        {    
        public:
            using  = ECSComponentGroup<Component..., Reject...>;
            using OwnedRepoType = ECSBatTransformList<ComponentRepo*, Component...>::Type;
            using RejectedRepoType = ECSBatTransformList<ComponentRepo*, Reject...>::Type;
        public:
            //function
            ECSComponentGroup() = default;
            ECSComponentGroup(OwnedRepoType owned, RejectedRepoType reject):owned_repos_(owned), rejected_repos(reject)  {
                std::apply([this](auto...* repo) { ((repo->OnConstruct() += DelegateLib::MakeDelegate(this, &ThisType::OnContruct); repo->OnRelease() += DelegateLib::MakeDelegate(this, &ThisType::OnRelease); ). ...); }, owned_repos_);
                //initializer group
                auto* cand_repo = std::get<0>(owned_repos_);
                for (auto entt : cand_repo) {
                    OnConstruct(entt);
                }
            }
            ~ECSComponentGroup() {
                std::apply([this](auto...* repo) { ((repo->OnConstruct() -= DelegateLib::MakeDelegate(this, &ThisType::OnContruct); repo->OnRelease() -= DelegateLib::MakeDelegate(this, &ThisType::OnRelease);), ...); }, owned_repos_);
                //todo world remove group
                //RemoveGroup<Component..., Reject...>();
            }
            auto Begin() {
                return ECSComponentGroupIterator<Component...>(0u, owned_repos_);
            }
            auto End() {
                return ECSComponentGroupIterator<Component...>(group_cursor_, owned_repos_);
            }
            bool IsEmpty() const 
            {
                return group_cursor_ <= 0u;
            }
            explicit operator bool() const {
                return IsEmpty();
            }
            template<typename Type, typename ...Other>
            bool IsOwned()const {
                return OwnedType::Contain<Type, Other...>();
            }
            template<typename Func>
            void For(Func&& func) {
                for (auto iter = Begin(); iter != End(); ++iter) {
                    func(*iter);
                }
            }
            template<typename Func>
            void ParallelFor(Func& func, size_type count) {
                const auto work_per_thread = group_cursor_ / count;
                const auto& sub_group_work = [&, this](uint32_t group, uint32_t thread) {
                    for (auto n = work_per_thread * group * thread; n < std::min(group_cursor_, work_per_thread * (group * thread + 1)); ++n)
                    {
                        func(*(Begin() + n));
                    }
                    co_return;
                };
                Utils::Dispatch(sub_group_work, 1, count);
            }
        private:
            void OnContruct(Entity e) { //todo apply function error use
                if (std::apply([e](auto...* repo) { return repo->IsContain(e)&&... }, owned_repos_) &&
                    std::apply([e](auto...* repo) { return (0u + ... + repo->IsContain(e)) == 0u; }, rejected_repos_ )
                {
                    //todo for each repo swap e and group_cursor
                    std::apply([e](auto...* repo) { ((const auto index = repo->ToIndex(e); repo->Swap(index, group_cursor_);), ...); }, owned_repos_);
                    ++group_cursor_;
                }
            }
            void OnUpdate(Entity e) {
                LOG(INFO) << "nothing related to update delegate";
            }
            void OnRelease(Entity e) {
                assert(group_cursor_ >= 0u);
                const auto index = std::get<0>(component_repos_)->ToIndex(e); //the order of this after release op done??
                if (index >= 0u && index < group_cursor_) {
                    std::apply([e](auto* repo) { ((const auto index = repo->ToIndex(e); repo->Swap(index, group_cursor_);). ...); }, owned_repos_);
                    --group_cursor_;
                }
            }
        private:
            template<typename T>
            static constexpr auto value = IsTypeIncluded<T, Component...>::value;
            OwnedRepoType    owned_repos_;
            RejectedRepoType    rejected_repos_;
            volatile uint32_t    group_cursor_{ 0u };
        };

        template<typename ...Component>
        class ECSComponentQueryIterator
        {
        public:
            using IteratorType = Vector<Entity>::iterator;
            using ThisType = ECSComponentViewIterator<Component...>;
            using ValueType = decltype(std::forward_as_tuple<Component...>); //todo
            //iterator traits field
            using value_type = ValueType;
            using pointer = value_type*;
            using reference = value_type;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            ECSComponentQueryIterator() = default;
            explicit ECSComponentQueryIterator(size_type iter, ):iter_(iter) {

            }

            ECSComponentQueryIterator& operator++() {
                //todo
                ++iter_;
                return *this;
            }
            ECSComponentQueryIterator operator++(int) {
                ECSComponentViewIterator temp{ *this };
                ++(*this);
                return temp;
            }
            auto operator*() {
                return *operator->();
            }
            auto operator->() {
                return std::apply([entity = iter_](auto...* repo) { return std::make_tuple(repo->GetComponent(entity), ...); }, component_repos_);
            }
            template<typename ...ComponentsLHS, typename ...ComponentsRHS>
            friend constexpr bool operator==(const ECSComponentViewIterator<ComponentsLHS...>& lhs, const ECSComponentViewIterator<ComponentsRHS...>& rhs) {
                static_assert(); //todo
                return lhs.iter_ == rhs.iter_;
            }

            template<typename ...ComponentsLHS, typename ...ComponentsRHS>
            friend constexpr bool operator!=(const ECSComponentViewIterator<ComponentsLHS...>& lhs, const ECSComponentViewIterator<ComponentsRHS...>& rhs) {
                return !(lhs == rhs);
            }
        private:
            //member field
            IteratorType iter_;
            std::tuple<ComponentRepo<Component>*, ...> component_repos_;
        };

        //todo support WithOut instead of excluced list and filter logic
        template<typename ...Reject>
        struct WithOut
        {
            bool operator()(void) const
            {

            }
        };

        template<typename ...>
        struct Filter
        {
            bool operator()(void) const
            {

            }
        };

        template<typename, typename>
        class ECSComponentQuery;

        template<typename ...Component, typename ...Reject>
        class ECSComponentQuery<ECSOwnedList<Component...>, ECSExcludedList<Reject...>>
        {
        public:
            using OwnedType = ECSOwnedList<Component...>;
            using RejectedType = ECSExcludedList<Reject...>;
            using OwnedRepoType = ECSBatTransformList<ComponentRepo*, Component...>::Type;
            using RejectedRepoType = ECSBatTransformList<ComponentRepo*, Reject...>::Type;
            using ViewType = OwnedType;

            template<typename Filter>
            requires std::is_same_v<bool, decltype(declval<Filter>())>
            decltype(auto) Filter(Filter&& filter) {
                eastl::remove_if(entities_.begin(), entities_.end(), filter);
                return *this;
            }
            template<typename Func>
            void For(Func&& func) {
                for (const auto& e : entities_) {
                    func(e); //todo
                }
            }
            template<typename Func>
            void ParallelFor(Func&& func, size_type count) {
                const auto work_per_thread = entities_.size() / count;
                const auto& sub_group_work = [&, this](uint32_t group, uint32_t thread) {
                    for (const auto n = group * thread * work_per_thread; n < std::min(entities_.size(), (group * thread + 1) * work_per_thread); ++n) {
                        func(entities_[n]);
                    }
                    co_return;
                };
                Utils::Dispatch(sub_group_work, 1, count);
            }
            explicit ECSComponentQuery(OwnedRepoType owned, RejectedRepoType reject):owned_repos_(owned), rejected_repos_(reject) {
                entities_.clear();
                auto* cand_repo = owned_repos_.get<0>();
                assert(nullptr != cand_repo && "repo for component is null");
                for (const auto ent : cand_repo->GetEntities()) { //todo
                    if (std::apply([ent, this](auto...* repo) { return (repo->HasComponent(ent)&&...); }, owned_repos_)
                        && std::apply([ent, this](auto ...* repo) { return (0u + ... + repo->HasComponent(ent)) == 0u; }, rejected_repos_))
                    {
                        entities_.emplace_back(ent);
                    }
                }
            }
            decltype(auto) Begin() {
                return ECSComponentQueryIterator<Component...>(entities_.begin(), owned_repos_);
            }
            decltype(auto) End() {
                return ECSComponentQueryIterator<Component...>(entities_,end(), owned_repos_);
            }
            bool IsEmpty() const {
                return entities_.empty();
            }
            explicit operator bool()const {
                return IsEmpty();
            }
            size_type Size()const {
                return entities_.size();
            }
        private:
            Vector<Entity>  entities_;
            OwnedRepoType   owned_repos_;
            RejectedRepoType    rejected_repos_;
        };

        template<class DataType>
        struct ECSSharedComponent
        {
            std::atomic_uint32_t    ref_count_{ 0u };
            DataType    data_;
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

        struct ECSSystemUpdateContext {
            float   delta_time_{ 0.f };//in seconds
            uint64_t    frame_index_{ 0u }; //frame index
            EUpdatePhase    phase_{ EUpdatePhase::eFrameStart };
        };

        template<typename ...Component>
        class ECSObserver
        {
            //todo
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
        class ECSComponentCommandBuffer
        {
        public:
            using CmdBufferContainer = Vector<ECSComponentCommandIR, std::allocator_traits<Allocator>::rebind_alloc<ECSComponentCommandIR>>;
            using ConstIterator = CmdBufferContainer::const_iterator;

            explicit ECSComponentCommandBuffer(Allocator& alloc, size_type int_size=1u):cmd_buffer_(int_size, alloc) {
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
            std::atomic_bool    write_lock_;
            CmdBufferContainer    cmd_buffers_;
        };

        template<typename Allocator, typename Function>
        static inline void EnqueueCommandBuffer(ECSComponentCommandBuffer<Allocator>& command_buffer, String& debug_name, ECSComponentCommandIR::EType type, Function&& function) {
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
            std::atomic_uint32_t    tick_counter_{ 0u }; //atomic to sync
        };

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
            SmallVector<ECSSystem*>    sub_sys_;
        };

        template<typename Allocator=Utils::LinearAllocator<void>, typename ...T>
        class ECSAdmin : public MessageQueue<T...>
        {
        public:
            using ThisType = ECSAdmin<T...>;
        public:
            template<typename System, typename... Args>
            requires std::is_convertible_v(ECSSystem, System)
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
            requires std::is_base_of_v(ECSSystem, System)
            System* GetSystem(auto key = std::type_index(typeid(System))) {
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

            template<typename Component...>
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

            template<typename ...Component, typename ...Reject>
            ECSComponentQuery<Component..., Reject...> Query() {
                auto owned_repos = GetComponentRepo<Component...>();
                if (std::apply([](auto...* repo) { return (repo!=nullptr&&...); }, owned_repos))
                {
                    auto rejected_repos = GetComponentRepo<Reject...>();
                    return { owned_repos, rejected_repos };
                }
                LOG(ERROR) << "the query disired not existed";
            }

            template<typename ...Component, typename ...Reject>
            ECSComponentGroup<Component..., Reject...>* Group(bool try_create = true) {
                using TargetGroupType = ECSComponentGroup<Component..., Reject...>;
                const auto target_key = std::type_index(typeid(TargetGroupType);
                if (auto iter = groups_.find(target_key)) {
                    return static_cast<TargetGroupType*>(iter->second);
                }
                else if (try_create) {
                    auto owned_repos = GetComponentRepo<Component...>();
                    auto rejected_repos = GetComponentRepo<Reject...>();
                    groups_.insert(std::make_pair(target_key, { owned_repos, rejected_repos }));
                    return
                }
                return nullptr;
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

            EntityType CreateEntity() {
                return EntityType(this);
            }

            template<typename Component, typename... Args>
            void AddComponent(Entity e, Args&&... args) {
                auto it = component_data_.find(typeid(Component));
                assert(it != component_data_.end());
                it->second->Add(std::forward<Args...>(args));
            }
            //component manager interface
            template<typename Component>
            Component& GetComponent(Entity e) {
                auto it = component_data_.find(typeid(Component));
                assert(component_data_.end() != it);
                const auto index = it->second->LookUp(e);
                return it->second->Element(index); //todo return null data
            }
            template<typename Component>
            void RemoveComponent(Entity e) {
                auto it = component_data_.find(typeid(Component));
                if (it != component_data_.end()) {
                    it->second->Remove(e);
                }
            }
            template<typename Component, typename... Args>
            void UpdateOrCreateComponent(Entity e,  Args... args) {
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
            template<typename Component, typename,,, Args>
            void AddSingletonComponent(Args&&... args) {
                if (!HasSingletonComponent<Component>()) {
                    using ComponentAllocator = std::allocator_traits<Allocator>::rebind_alloc<Component>;
                    auto* data = std::allocator_traits<ComponentAllocator>::allocate(alloc_, 1);
                    std::allocator_traits<ComponentAllocator>::construct(alloc_, data, std::forward<Args>(args)...);
                    single_data_.insert(std::make_pair(typeid(Component), data)); //todo
                    return;
                }
                LOG(ERROR) << "already has one singleton instance";
            }

            template<typename Component>
            Component* GetSingletonComponent() {
                assert(HasSingletonComponent<Component>());
                return static_cast<Component*>(singleton_data_[typeid(Component)]);
            }

            template<typename Component>
            bool HasSingletonComponent() const {
                auto iter = singleton_data_.find(typeid(Component));
                return iter != singleton_data_.end();
            }

            template<typename Component>
            void RemoveSingletonComponent() {
                assert(HasSingltonComponent<Component>());
                auto iter = singleton_data_.find(typeid(Component));
                if (iter != singleton_data_.end()) {
                    del iter->second;
                    singleton_data_.erase(iter);
                }
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
        private:
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
                        for (auto [comp_id, comp_data] : component_data_) {
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

            void DelEntity(Entity e) {
                for (auto [_, comp_repo] : components_data_) {
                    comp_repo->Remove(e); //todo slow
                }
                entity_manager_.Release(e);
            }
            //copies an existing entity and creates a new entity from that copy
            Entity Clone(Entity prefab) {
                auto new_entity = NewEntity();
                for (auto [_, comp_repo] : components_data_) {
                    if (comp_repo->IsCompoenentExisit(prefab)) {
                        AddComponent(new_entity, GetComponent(prefab));
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
            //todo fixme type info bugs ??? //https://skypjack.github.io/2020-03-14-ecs-baf-part-8/
            Map<std::type_index, ArcheTypeTableBase*> archetype_tables_;
            Map<std::type_index, ComponentRepoBase*> components_data_;
            Map<std::type_index, void*>    singleton_data_;

            //archetype table lookuptable
            Map<std::type_index, std::type_index> component_to_archetype_;
            Map<std::type_index, std::type_index> component_tuple_to_archetype_;

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
        };
    }
}