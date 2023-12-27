#pragma once

namespace Shard
{
	namespace Utils
	{
		template<typename T>
		class Handle
		{
		public:
			explicit Handle(uint32_t index = 0, uint8_t generation=0) : index_(index), generation_(generation) {}
			bool IsValid()const { return generation_ != 0; }
			uint32_t Index()const { return index_; }
			uint8_t	Generation()const { return generation_; }
			operator uint32_t()const { return Index(); }
			Handle operator++(int) {
				Handle temp(*this);
				generation_++;
				return temp;
			}
			Handle& operator++() { 
				++generation_;
				return *this;
			}
			//ambiguity with ++
			bool operator==(Handle other)const { return Index() == other.Index(); }
			bool operator<=(Handle other)const { return Index() <= other.Index(); }
			bool operator>=(Handle other)const { return Index() >= other.Index(); }
			bool operator<(Handle other)const { return Index() < other.Index(); }
			bool operator>(Handle other)const { return Index() > other.Index(); }
		private:
			uint32_t	index_{ 0u };
			uint8_t		generation_{ 0u };
		};

		template<typename T>
		class HandleManager
		{
		public:
			using Type = T;
			using Handle = Handle<Type>;
			virtual Handle Alloc() {
				auto index = GetFreeHandle();
				if (index != -1) {
					generation_[index] = 1u;
					return { index, 1u };
				}
				generation_.push_back(1u);
				return { generation_.size() - 1, 1u };
			}
			virtual void Free(Handle handle) {
				generation_[handle.Index()]++;
				free_list_.push_back(handle.Index());
			}
			bool IsValid(Handle handle) {
				return handle.IsValid() && generation_[uint32_t(handle)] == handle.Generation();
			}
			virtual ~HandleManager() {}
		protected:
			uint32_t GetFreeHandle() const {
				if (free_list_.empty()) {
					return -1;
				}
				else
				{
					auto index = free_list_.back();
					free_list_.pop_back();
					return index;
				}
			}
			void StoreGeneration(Handle handle) {
				generation[handle.Index()] = handle.Generation();
			}
		protected:
			Vector<uint8_t>		generation_;
			Vector<uint32_t>	free_list_;
		};

		template<typename Allocator, typename T>
		class ResourceManager : public HandleManager<T>
		{
		public:
			using Type = HandleManager<T>::Type;
			using Handle = HandleManager<T>::Handle;
			ResourceManager(Allocator& alloc) : alloc_(alloc) {}
			template<typename DerivedType = Type, typename... Args>
			requires std::is_convertible_v<DerivedType*, Type*>
			Handle Alloc(Args&&... args) {
				auto* ptr = alloc_.AllocNoDestruct<DerivedType>(std::forward<Args>(args)...);
				storage_.push_back(static_cast<Type*>(ptr));
				generation_.push_back(1u);
				return { generation_.size()-1, 1u };
			}
			//FIXME
			Handle Insert(T* data) {
				auto index = GetFreeHandle();
				T* ptr = nullptr;
				if (index != -1) {
					new (storage_[index])T(data);
					generation_[index] = 1;
					return Handle<T>(index, 1);
				}
				else 
					return Alloc(data);
			}
			void Free(Handle handle) override{
				HandleManager<T>::Free(handle);
				storage_[handle]->~T();//FIXME
			}
			//FIXME
			Type* Get(Handle handle) {
				if (IsValid(handle)) {
					return storage_[handle];
				}
				return nullptr;
			}
		private:
			Allocator&			alloc_;
			//no destruct T
			Vector<T*>			storage_;
		};

	}
}

