#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include "Delegate/Delegate/Delegate.h"
#include <limits>
#include <type_traits>

//http://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system.html
//https://docs.unity3d.com/Packages/com.unity.entities@0.17/manual/ecs_core.html
//https://www.youtube.com/watch?v=jjEsB611kxs

namespace Shard
{
	namespace Utils
	{ 

		template<typename, typename = void>
		struct has_update_func : std::false_type {};
		template<typename System>
		struct has_update_func<System, std::void_t<decltype(std::declval<System>.Update())>> : std::true_type {};
		template<typename, typename = void>
		struct has_message : std::false_type {};
		template<typename System, typename MessageType>
		struct has_message<System, std::void_t<decltype(std::declval<System>().Deal(std::declval<MessageType>()))> > : std::true_type {};
		template<typename, typename = void>
		struct has_component : std::false_type{};
		template<typename System>
		struct has_component<System, std::void_t<decltype(std::declval<System>().components_)>> : std::true_type {};
		//component system operation
		
		template<typename, typename>
		struct is_applicable : std::false_type {};
		template<typename Func, template<typename...> class Tuple, typename... Args>
		struct is_applicable<Func, const Tuple<Args...>> : std::is_invocable<Func, Args...> {};

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
			operator bool() const {
				return generation_ > 0;
			}
			uint32_t	id_ : 20;
			uint32_t	generation_ : 12;
			static const Entity Null;
		};

		class MINIT_API EntityManager
		{
		public:
			using VersionType = uint16_t;
			Entity Generate();
			void Release(Entity entity);
			VersionType Version(const Entity& entity)const;
			bool IsAlive(const Entity& entity) const;
		public:
			Vector<VersionType>	generation_;
			List<uint32_t>	free_indices_;
		};

		template<typename ValueType, typename Allocator>
		class SparseSet
		{
		public:
			using ValueType = ValueType;
			using SizeType = Vector<ValueType>::size_type;
			using Iterator = Vector<ValueType>::iterator;
			using ConstIterator = Vector<ValueType>::const_iterator;
			using ThisType = SparseSet<SizeType, ValueType>;
			using AllocatorType = std::allocator_traits<Allocator>::rbind_alloc<ValueType>;

			SparseSet() :SparseSet(0u, AllocatorType()) {}
			explicit SparseSet(SizeType size, AllocatorType& alloc): allocator_(alloc) {
				Resize(size);
			}
			void Resize(SizeType size) {
				const auto old_size = Size();
				sparse_.resize(size);
				if (old_size > size) {
					//to do
				}
				const auto page_size = std::ceil(float(old_size) / SparsePage::PAGE_SIZE);
				//todo renew pages
			}
			void Swap(ThisType& rhs) {
				std::swap(dense_, rhs.dense_);
				std::swap(sparse_pages_, rhs.sparse_pages_);
				//todo allocator
			}
			void Insert(ValueType i) {
				dense_.push_back(i);
				sparse_[i] = dense_.size() - 1;
			}
			void Erase(ValueType i) {
				const auto dense_index = sparse_[i];
				sparse_[i] = -1;
				if (dense_index != dense_, size() - 1) {
					const auto dense_back = dense_.back();
					sparse_[dense_back] = dense_index;
					dense_[dense_index] = dense_back;
				}
				dense_.pop_back();
			}
			void Erase(const Iterator& iter) {
				Erase(*iter);
			}
			void Clear() {
				dense_.clear();
				for (auto* page : sparse_pages_) {
					alloacor_.DeAlloc(page);
					page_ = nullptr;
				}
			}
			SizeType Size()const {
				//return sparse_.size();
			}
			SizeType Count()const {
				return dense_.size();
			}
			bool Test(ValueType i) {
				//return sparse_[i] != -1;
			}
			bool IsEmpty()const {
				return dense_.empty();
			}
		private:
			struct SparsePage
			{
				static constexpr auto PAGE_SIZE = 256u;
				static constexpr auto PAGE_COUNT = 128u;
				ValueType	data_[PAGE_SIZE];
			};
			void SetSparseValue(ValueType pos, ValueType value) {
				const auto page_index = std::ceil(pos / SparsePage::PAGE_SIZE);
				if (sparse_[page_index] == nullptr) {
					sparse_[page_index] = allocator_.Alloc();
				}
				const auto inner_index = pos % SparsePage::PAGE_SIZE;
				sparse_[page_index]->data_[inner_index] = value;
			}
		private:
			Vector<ValueType, AllocatorType>	dense_;
			SmallVector<SparsePage*, SparsePage::PAGE_COUNT>	sparse_pages_;
			AllocatorType&	allocator_;
		};

		//2. model graph in between diff comp in diff entity
		//1 or 2 model which to realize
		class ComponentInterface
		{
		public:
			struct Instance {
				uint32_t	index_{ std::numeric_limits<uint32_t>::max() };
			};
			ComponentInterface(LinearAllocator* allocator) : allocator_(allocator) {}
			Instance MakeInstance(uint32_t i) { return { i };}
			Instance Insert(Entity entity);
			Instance LookUp(Entity entity) const;
			bool Contain(Entity entity)const { return LookUp(entity).index_ != -1; }
			//garbage collection fixme when to call
			void GC(const EntityManager& entity_manager);
			virtual void Destroy(Entity entity) = 0;
			virtual ~ComponentInterface() = default;
			//static RegisterComponentType()
		protected:
			void* Alloc(size_t size);
		protected:
			LinearAllocator*	allocator_{ nullptr };
			uint32_t	component_identity_;
			Map<Entity, Instance>	id_map_;
		};

		template<typename Component>
		class ComponentRepo : public ComponentInterface
		{
		public:
			using Type = Component;
			using ThisType = ComponentRepo<Component>;
			void Reverse(uint32_t size) {
				auto ptr = allocator_->Alloc(size*sizeof(Component));
				if (nullptr != component_data_) {
					auto cpy_size = id_map_.size() > size ? size : id_map_.size();
					memcpy(ptr, component_data_, sizeof(Component) * cpy_size);
				}
				component_data_ = ptr;
				capacity_ = size;
			}
			void Destroy(Entity e) override {
				auto index = LookUp(e);
				if (index != -1) {
					Remove(index);
				}
			}
			void Clear() {
				//todo
			}
			template<typename... Args>
			Type& Append(uint32_t index, Args&&... args) {
				if (++size_ > 0.9 * capacity_) {
					auto new_size = 2*capacity_; //FIXME
					Reverse(new_size);
				}
				Type* result = component_data_ + index;
				new(result)(std::forward<Args>(args)...);
				return result;
			}

			void Remove(uint32_t index)
			{
				//swap size-1 to index, fixme deal reference
				auto ptr = (component_data_ + index);
				if constexpr(ptr->) {
					Remove();
				}
				reinterpret_cast<Type*>(ptr)->~Type();
				*(component_data_ + index) = *(component_data_ + size_ - 1);
				--size_;

			}

			void Swap(uint32_t lhs, uint32_t rhs) {

			}

			//sort component
			template<typename BinaryCompare>
			void Sort(BinaryCompare&& compare_op) {

			}

			//sort component order as target component repo
			template<typename OtherComponent>
			void SortAs(const ComponentRepo<OtherComponent>& target) {

			}


			Entity ToEntity(uint32_t index) {
				//todo 
			}

			Type& Element(uint32_t index)
			{
				assert(index < size_);
				return *(component_data_ + index);
			}

			const Type& Element(uint32_t index) const
			{
				assert(index < size_);
				return *(component_data_ + index);
			}

		private:
			SparseSet<uint32_t>	sparse_set_;
			Type* component_data_{ nullptr };
			//delegate 
			Delegate<void(ThisType*, Entity)>	on_construct_;
			Delegate<void(ThisType*, Entity)>	on_update_;
			Delegate<void(ThisType*, Entity)>	on_release_;
		};

		template<typename SharedComponent>
		class SharedComponentRepo : public ComponentInterface
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

		template<typename ...T>
		class MessageQueue
		{
		public:
			//message size 128
			struct Message
			{
				enum class Flags : uint16_t
				{
					eNone		= 0x0,
					eRecursive	= 0x1,
				};
				uint32_t	message_type_ : 16;
				uint32_t	message_flags_ : 16;
				float		time_{ 0 };
				uint8_t		payload_[128 - sizeof(uint32_t) - sizeof(float)];
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
			using ThisType = ECSComponentList<Component...>;
			static constexpr auto SIZE = sizeof...(Component);
			template<typename ...TT>
			static bool Contain() {
				if constexpr (sizeof...(TT) == 1u) {
					return IsTypeIncluded<TT, Component...>::value;
				}
				else
				{
					return (Contain<TT>()&&...);
				}
			}
			constexpr ECSComponentList() = default;
		};

		//list name alias
		template<typename ...Component>
		struct ECSExcludedList final : ECSComponentList<Component...> {};
		template<typename ...Component>
		struct ECSOwnedList final : ECSComponentList<Component...>{};

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
				return	operator->();
			}
			[[nodiscard]] auto operator->() {
				return std::make_tuple(std::apply([entity = iter_](auto* repo) { return repo->GetComponent(entity); }, components_repos_)); //todo
			}
			template<typename ...ComponentLHS, typename ...ComponentRHS>
			friend constexpr bool operator==(const ECSComponentGroupIterator<ComponentLHS...>& lhs, const ECSComponentGroupIterator<ComponentRHS...>& rhs) {

			}

			template<typename ...ComponentLHS, typename ...ComponentRHS>
			friend constexpr bool operator!=(const ECSComponentGroupIterator<ComponentLHS...>& lhs, const ECSComponentGroupIterator<ComponentRHS...>& rhs) {
				return !(lhs == rhs);
			}
		private:
			bool IsValid() const {
				return iter_ != iter_last_; //todo
			}
		private:
			//member field
			size_type	iter_;
			std::tuple<ComponentRepo<Component>*...> components_repos_;

		};
		
		template<typename, typename>
		class ECSComponentGroup;
		/*group component together and pointer to position by cursor*/
		template<typename ECSAdmin, typename ...Component>
		class ECSComponentGroup<ECSAdmin, ECSOwnedList<Component...>>
		{	
		public:
			using OwnedType = ECSOwnedList<Component...>;
		public:
			//function
			ECSComponentGroup() = default;
			ECSComponentGroup(ECSAdmin* admin) {
				component_repos_ = std::make_tuple(admin->xxx(), ...);
				//todo deltegate 
			}
			auto Begin() {
				return ECSComponentGroupIterator<Component...>(0u, component_repos_);
			}
			auto End() {
				return ECSComponentGroupIterator<Component...>(group_cursor_, component_repos_);
			}
			template<typename Type, typename ...Other>
			bool IsOwned()const {
				return OwnedType::Contain<Type, Other...>();
				//const auto owned[]{ value<std::remove_cv_t<Type>>, value<std::remove_cv_t<Other>>... };
				//return std::all_of(owned, owned+1u+sizeof...(Other), [&](auto val) {return val});
			}
		private:
			void OnContruct(Entity e) {

			}
			void OnUpdate(Entity e) {

			}
			void OnRelease(Entity e) {
				if (e < group_cursor_) {

				}
			}
			void Sort() {

			}
			void Swap(Entity lhs, Entity rhs) {

			}
		private:
			template<typename T>
			static constexpr auto value = IsTypeIncluded<T, Component...>::value;
			std::tuple<ComponentRepo<Component>*...> component_repos_;
			uint32_t	group_cursor_{ 0u };
		};

		template<typename ...Component>
		class ECSComponentViewIterator
		{
		public:
			using ThisType = ECSComponentViewIterator<Component...>;
			using ValueType = decltype(std::forward_as_tuple<Component...>); //todo
			//iterator traits field
			using value_type = ValueType;
			using pointer = value_type*;
			using reference = value_type;
			using difference_type = std::ptrdiff_t;
			using iterator_category = std::forward_iterator_tag;

			ECSComponentViewIterator() = default;
			explicit ECSComponentViewIterator() {

			}

			ECSComponentViewIterator& operator++() {
				//todo
				return *this;
			}
			ECSComponentViewIterator operator++(int) {
				ECSComponentViewIterator temp{ *this };
				++(*this);
				return temp;
			}
			auto operator*() {
				return *operator->();
			}
			auto operator->() {
				return &nullptr;
			}
			template<typename ...ComponentsLHS, typename ...ComponentsRHS>
			friend constexpr bool operator==(const ECSComponentViewIterator<>& lhs, const ECSComponentViewIterator<>& rhs) {
				static_assert(); //todo
				return lhs.iter_ == rhs.iter_;
			}

			template<typename ...ComponentsLHS, typename ...ComponentsRHS>
			friend constexpr bool operator!=(const ECSComponentViewIterator<>& lhs, const ECSComponentViewIterator<>& rhs) {
				return !(lhs == rhs);
			}
		private:
			//member field
			size_type iter_;
		};

		template<typename ECSAdmin, typename ...Component>
		class ECSComponentView
		{
		public:
			using ViewType = std::tuple<Component...>;

			template<typename Func>
			void ForEach() {
				for (const auto& e : entities_) {
					Func(); //todo
				}
			}
			ECSComponentView(ECSAdmin* admin):ecs_admin_(admin) {
				//collect entities_
			}
			decltype(auto) Begin() {
				return ECSComponentViewIterator<Component...>();
			}
			decltype(auto) End() {
				return ECSComponentViewIterator<Component...>();
			}
		private:
			Vector<Entity>	entities_;
			ECSAdmin* ecs_admin_{ nullptr };
		};

		template<typename ECSAdmin, typename Component>
		class ECSComponentView
		{
		public:
			template<typename Func>
			void ForEach() {

			}
			ECSComponentView(ECSAdmin* admin) :ecs_admin_(admin) {}
		private:
			ECSAdmin* ecs_amdin_{ nullptr };
		};

		struct ECSSharedComponent
		{
			std::atomic_uint32_t	ref_counter_{ 0u };
			uint32_t	index_{ 0u };
		};

		class ECSSystem
		{
		public:
			virtual void OnInit() = 0;
			virtual void OnUnit() = 0;
			virtual void OnUpdate(float dt) = 0;
			virtual ~ECSSystem() = default;
		public:
			uint32_t	prior_order_{ 0u };
		};


		template<typename ...T>
		class ECSAdmin : public MessageQueue<T...>
		{
		public:
			using ThisType = ECSAdmin<T...>;
			template<typename System, typename... Args>
			System* RegisterSystem(Args&&... args) {
				auto ptr = new System(std::forward<Args>(args)...);
				systems_.insert(std::make_pair());
				if constexpr (has_component<System>) {
					RegisterComponentMessage(ptr, ,std::bool_constant<>());
				}
				//FIXME message handler
				{
					//auto dummy[] = { (RegisterMessageHandler<System, T...>(ptr), 0)... };
				}
				return ptr;
			}
			template<typename Component>
			void RegisterComponent(ComponentRepo<Component>* component_repo) {
				components_data_.insert(std::make_pair(typeid(Component), component_repo));
			}
			template<typename Component>
			void RegisterComponent() {
				auto comp_repo = new ComponentRepo<Component>; //todo 
				RegisterComponent(comp_repo);
			}
			template<typename Component>
			ComponentRepo<Component>* GetComponentRepo() {
				return dynamic_cast<ComponentRepo<Component>*>(components_data_[typeid(Component)]);
			}

			template<typename Component, typename BinaryCompare>
			void Sort(BinrayCompare& compare_op) {
				GetComponentRepo<Component>()->Sort(compare_op);
			}

			template<typename Component, typename ComponentReferred>
			void Sort() {
				GetComponentRepo<Component>()->Sort();//todo
			}

			template<typename ...Component>
			ECSComponentView<Component...> View() {
				//todo
			}

			template<typename ...Component>
			ECSComponentGroup<Component...> Group() {

			}

			template<typename ...Component>
			ECSComponentGroup<Component...> GroupIfExsit() {

			}

			template<typename ...Component>
			void Clear() {
				if constexpr (sizeof...(Component) == 0u) {
					for (auto [_, comp] : components_data_) {
						//todo 
						delete comp;
					}
					for (auto [_, sys] : systems_) {
						delete sys;
					}
				}
				else {
					if constexpr (sizeof...(Component) == 1u) {
						GetComponentRepo<Component>()->Clear();
					}
					else {
						(Clear<Component>(), ...); //fold express
					}
				}
			}
			virtual ~ECSAdmin() {
				Clear();
			}

			Entity NewEntity() {
				auto entity = entity_manager_.Generate();
				return entity;
			}

			void DelEntity(Entity e) {
				for (auto [_, comp_repo] : components_data_) {
					comp_repo->Remove(e);
				}
				entity_manager_.Release(e);
			}
			//copies an existing entity and creates a new entity from that copy
			Entity Clone(Entity entity) {
				auto new_entity = NewEntity();
				for (auto [_, comp_repo] : components_data_) {
					if (comp_repo->IsCompoenentExisit(entity)) {
						AddComponent(new_entity, GetComponent(entity));
					}
				}
			}

			template<typename Component, typename... Args>
			void AddComponent(Entity e, Args&&... args) {
				auto it = component_data_.find(typeid(Component));
				assert(it != component_data_.end());
				auto comp_iterface = it->second;
				//FIXME
				comp_iterface->Add(std::forward<Args...>(args));
			}
			//component manager interface
			template<typename Component>
			Component& GetComponent(Entity e) {
				auto it = component_data_.find(typeid(Component));
				assert(component_data_.end() != it);
				const auto index = it->second->LookUp(e);
				return it->second->Element(index);
			}
			template<typename Component>
			void RemoveComponent(Entity e) {
				auto it = component_data_.find(typeid(Component));
				if (it != component_data_.end()) {
					it->second->
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
			template<typename System>
			System* GetOrCreateSystem() {
				return nullptr;
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
					RegisterHandler(, [&](Message& mess) {
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
		private:
			EntityManager entity_manager_;
			//todo fixme type info bugs ??? //https://skypjack.github.io/2020-03-14-ecs-baf-part-8/
			Map<std::type_index, ComponentInterface*> components_data_;
			struct Dummy {};
			template<typename System>
			requires has_update_func<System>::value
			struct SystemInstance : public Dummy {
				using Type = System;
				Type* handle_{ nullptr };
			};
			template <typename... Components>
			struct ComponenentCollector
			{
				static constexpr const uint32_t COMP_SIZE = sizeof...(Components);
				ComponentCollector(ThisType& world) {
					Fill<0, Components...>(world);
				}
				ComponentInterface* components_data_[COMP_SIZE];
				template<uint32_t index>
				ComponentInterface* Get() { return components_data_[index]; }
				template <uint32_t index>
				void Fill(Map<std::type_index, ComponentInterface*>& components) {
					return;
				}
				template <uint32_t index, typename T, typename... Ts>
				void Fill(Map<std::type_index, ComponentInterface*>& components) {
					components_data_[index] = components[typeid(T)];
					Fill<index + 1, Ts...>(components);
				}
			};

			Map<std::type_index, Dummy*> systems_;
			//ecs group define 
			Map<, ECSComponentGroup<> > groups_;
		};
	}
}