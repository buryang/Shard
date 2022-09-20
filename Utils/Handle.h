#pragma once

namespace MetaInit
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
			uint32_t	index_{ 0 };
			uint8_t		generation_{ 0 };
		};

		template<typename Allocator, typename T>
		class ResourceManager
		{
		public:
			using Type = T;
			using Handle = Handle<Type>;
			ResourceManager(Allocator& alloc) : alloc_(alloc) {}
			template<typename DerivedType = Type, typename... Args>
			requires std::is_convertible_v<DerivedType*, Type*>
			Handle<T> Alloc(Args&&... args) {
				auto* ptr = alloc_.AllocNoDestruct<DerivedType>(std::forward<Args>(args)...);
				storage_.push_back(static_cast<Type*>(ptr));
				generation_.push_back(1);
				return { storage_.size()-1, 1 };
			}
			Handle<T> Insert(T* data) {
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
			//FIXME
			Type* Get(Handle<T> handle) {
				if (IsValid(handle)) {
					return storage_[handle];
				}
				return nullptr;
			}
			void Free(Handle<T> handle) {
				generation_[handle.Index()]++;
				free_list_.push_back(handle.Index());
			}
			bool IsValid(Handle<T> handle) {
				return handle.IsValid() && generation_[uint32_t(handle)] == handle.Generation();
			}
		private:
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
			void StoreGeneration(Handle<T> handle) {
				generation[handle.Index()] = handle.Generation();
			}
		private:
			Allocator&			alloc_;
			//no destruct T
			Vector<T*>			storage_;
			Vector<uint8_t>		generation_;
			Vector<uint32_t>	free_list_;
		};

	}
}

