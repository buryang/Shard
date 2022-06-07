#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Memory.h"
#include <limits>

//http://bitsquid.blogspot.com/2014/10/building-data-oriented-entity-system.html
//https://docs.unity3d.com/Packages/com.unity.entities@0.17/manual/ecs_core.html

namespace MetaInit
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
		struct has_message < System, std::void_t<> : std::true_type {};
		template<typename, typename = void>
		struct has_component : std::false_type{};
		template<typename System>
		struct has_component<System, std::void_t<decltype(std::declval<System>.components_)>> : std::true_type {};
		//component system operation
		
		template<typename, typename>
		struct is_applicable : std::false_type {};
		template<typename Func, template<typename...> class Tuple, typename... Args>
		struct is_applicable<Func, const Tuple<Args...>> : std::is_invocable<Func, Args...> {};

		struct Entity
		{
			Entity() :id_(-1), generation_(0) {}
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
			uint32_t	id_ : 24;
			uint32_t	generation_ : 8;
		};

		class MINIT_API EntityManager
		{
		public:
			Entity Generate();
			void Release(Entity entity);
			bool IsAlive(const Entity& entity) const;
		public:
			Vector<uint8_t>		generation_;
			Vector<uint32_t>	free_indices_;
			static constexpr uint32_t MIN_FREE_INDICES = 100;//FIXME
		};

		//1. entity graph
		class MINIT_API EntityGraphManager
		{
		public:
			Entity Generate();
			void Release(Entity entity);
			bool IsAlive(Entity entity) const;
			Entity Parent(Entity entity) const;
			bool Attach(Entity parent, Entity child);
		private:
			struct InnerEntity
			{
				uint32_t	parent_;
				uint32_t	first_child_;
				uint32_t	next_sibling_;
				uint32_t	prev_sibling_;
			};
			Vector<InnerEntity>		entities_;
		};

		//2. model graph in between diff comp in diff entity
		//1 or 2 model which to realize
		class MINIT_API ComponentInterface
		{
		public:
			struct Instance {
				uint32_t	index_{ std::numeric_limits<uint32_t>::max() };
			};
			ComponentInterface(Allocator* allocator) : allocator_(allocator) {}
			Instance MakeInstance(uint32_t i) { return { i };}
			Instance Insert(Entity entity);
			Instance LookUp(Entity entity) const;
			//garbage collection fixme when to call
			void GC(const EntityManager& entity_manager);
			virtual void Destroy(Entity entity) = 0;
			virtual ~ComponentInterface() {}
			//static RegisterComponentType()
		protected:
			void* Alloc(size_t size);
		protected:
			Allocator*	allocator_{ nullptr };
			uint32_t	component_identity_;
			std::unordered_map<Entity, uint32_t>	id_map_;
		};

		template<typename Component>
		class ComponentRepo : public ComponentInterface
		{
		public:
			using Type = Component;
			void Reverse(uint32_t size) {
				auto ptr = allocator_->Alloc(size*sizeof(Component));
				if (nullptr != component_data_) {
					auto cpy_size = id_map_.size() > size ? size : id_map_.size();
					memcpy(ptr, component_data_, sizeof(Component) * cpy_size);
				}
				component_data_ = ptr;
				capacity_ = size;
			}
			void Destroy(Entity e) override;
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
				->~Type();
				*(component_data_ + index) = *(component_data_ + size_ - 1);
				--size_;

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
			uint32_t size_{ 0 };
			uint32_t capacity_{ 0 };
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
					assert(MessageType:: = message_type_);
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
			std::unordered_map<uint32_t, SmallVector<MessageHandler>> message_handlers_;
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


		template<typename ...T>
		class ECSWorld : public MessageQueue<T...>
		{
		public:
			using ThisType = ESCWorld<T...>;
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
			ComponentRepo<Component>* GetComponentRepo() {
				return dynamic_cast<omponentRepo<Component>*>(components_data_[typeid(Component)]);
			}
			void Clear() {
				for (auto comp : components_data_) {
					//todo 
				}
				for (auto sys : systems_) {
					delete sys.second;
				}
			}
			virtual ~ECSWorld() {
				Clear();
			}

			template<typename Component, typename... Args>
			void Add(Entity e, Args&&... args) {
				auto it = component_data_.find(typeid(Component));
				assert(it != component_data_.end());
				auto comp_iterface = it->second;
				//FIXME
				comp_iterface->Add(std::forward<Args...>(args));
			}
			//component manager interface
			template<typename Component>
			Component& Get(Entity e) {
				auto it = component_data_.find(typeid(Component));
				assert(component_data_.end() != it);
				const auto index = it->second->LookUp(e);
				return it->second->Element(index);
			}
			template<typename Component, typename... Args>
			Component& UpdateOrCreate(Entity e) {
				auto it = component_data_.find(typeid(Component));
				assert(component_data_.end() != it);
				const auto index = it->second->LookUp(e);
				if () {
					//todo
				}
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
			std::unordered_map<std::type_index, ComponentInterface*> components_data_;
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
				void Fill(std::unordered_map<std::type_index, ComponentInterface*>& components) {
					return;
				}
				template <uint32_t index, typename T, typename... Ts>
				void Fill(std::unordered_map<std::type_index, ComponentInterface*>& components) {
					components_data_[index] = components[typeid(T)];
					Fill<index + 1, Ts...>(components);
				}
			};

			std::unordered_map<std::type_index, Dummy*> systems_;
		};
	}
}