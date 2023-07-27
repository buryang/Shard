#pragma once
#include "Utils/CommonUtils.h"
#include "folly/memory/arena.h"

namespace Shard
{
	namespace Utils
	{
		class LinearAllocatorImpl final
		{
		public:
			explicit LinearAllocatorImpl(size_type capacity);
			void* Alloc(size_type size, size_type n);
			void DeAlloc(void* ptr, size_type n);
			void Reset();
			const size_type GetCapacity()const {
				return capacity_;
			}
			~LinearAllocatorImpl();
		private:
			size_type	capacity_{ 0u };
			size_type	offset_{ 0u };
			void* memory_{ nullptr };
		};

		template<class T>
		class LinearAllocator
		{
		public:
			//field for allocator_traits
			using value_type = T;
			using pointer = T*;
			using allocate = Alloc;
			using deallocate = DeAlloc;
			using max_size = GetCapacity;
			template<class U>
			struct rebind {
				using other = LinearAllocator<U>;
			};
			template<class>
			friend class LinearAllocator;

		public:
			explicit LinearAllocator(size_type capacity = 1024 * 1024) :impl_(std::make_shared<LinearAllocatorImpl>(capacity) {
			}
			template<class U>
			LinearAllocator(const LinearAllocator<U>& other) noexcept : impl_(other.impl_) //todo
			{

			}
			[[nodiscard]] pointer Alloc(size_type n = 1u) {
				pointer ptr = impl_->Alloc(sizeof(T), n);
				if (nullptr != ptr) {
					return reinterpret_cast<T*>(ptr);
				}
				return ptr;
			}
			template <typename... Args>
			[[nodiscard]] pointer AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc();
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(pointer ptr, size_type n) {
				impl_->DeAlloc(ptr, n * sizeof(T));
			}
			size_type GetCapacity()const {
				return impl_->GetCapacity() / sizeof(value_type);
			}
			~LinearAllocator() = default;
			void Reset() {
				impl_->Reset();
			}
			template<class U>
			friend bool operator==(const LinearAllocator& lhs, const LinearAllocator<U>& rhs) {
				return lhs.impl_.get() && lhs.impl_ == rhs.impl_;
			}
			template<class U>
			friend bool operator!=(const LinearAllocator& lhs, const LinearAllocator<U>& rhs) {
				return !(lhs == rhs);
			}
		private:
			std::shared_ptr<LinearAllocatorImpl>	impl_;
		};

		class StackAllocatorImpl final
		{
		public:
			StackAllocatorImpl() = default;
			explicit StackAllocatorImpl(size_type capacity);
			void* Alloc(size_type size, size_type n);
			void DeAlloc(void* ptr, size_type n);
			void Reset();
			~StackAllocatorImpl();
		private:
			size_type	capactity_{ 0u };
			size_type	offset_{ 0u };
			void* memory_{ nullptr };
		};

		template<class T>
		class StackAllocator
		{
		public:
			//field for allocator_traits
			using value_type = T;
			using allocate = Alloc;
			using deallocate = DeAlloc;
			template<class U>
			struct rebind
			{
				using other = StackAllocator<U>;
			};
			template<class>
			friend class StackAllocator;
		public:
			explicit StackAllocator(size_type capacity) :std::make_shared<StackAllocatorImpl>(capacity) {

			}
			template<class U>
			StackAllocator(const StackAllocator<U>& other) noexcept :impl_(other.impl_)
			{
			}
			[[nodiscard]] T* Alloc(size_type n = 1u) {
				return reinterpret_cast<T*>(impl_->Alloc(sizeof(T), n));
			}
			template <typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc();
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(T* ptr, size_type n) {
				std::destroy(ptr, ptr + n);
				impl_->DeAlloc(ptr, sizeof(T) * n);
			}
			void Reset() {
				impl_->Reset();
			}
			template<class U>
			friend bool operator==(const StackAllocator& lhs, const StackAllocator<U>& rhs) {
				return lhs.impl_.get() && lhs.impl_ == rhs.impl_;
			}
			template<class U>
			friend bool operator!=(const StackAllocator& lhs, const StackAllocator<U>& rhs) {
				return !(lhs == rhs);
			}
		private:
			std::shared_ptr<StackAllocatorImpl> impl_;
		};

		template<class T>
		class DoubleEndStackAllocator
		{
			//todo
		};

		template<typename T>
		class PooledAllocator //not copyable
		{
		public:
			using value_type = T;
		public:
			PooledAllocator() = default;
			explicit PooledAllocator(size_type element_count) {
				Init(element_count);
			}
			void Init(size_type element_count) {
				memory_ = ;; operator new(sizeof(Node) * element_count);
				head£ß = reinterpret_cast<Node*>(memory_);
				auto* tail = head_;
				//constructor free list
				for (auto n = 0; n < element_count; ++n, ++tail) {
					tail->next_ = tail + 1;
				}
				tail->next_ = nullptr;
			}
			//todo lock
			[[nodiscard]] T* Alloc() {
				auto* next = free_head_->next_;
				if (next == nullptr) {
					return nullptr;
				}
				std::swap(next, free_head_);
				return &(next->data_);
			}
			template <typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc();
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(T* ptr) {
				std::destroy(ptr, ptr + 1);//to do
				auto* node = reintepret_cast<Node*>(static_cast<uintptr_t>(ptr) - sizeof(Node*));
				node->next_ = free_head_;
				std::swap(node, free_head_);
			}
			~PooledAllocator() { ::operator delete(memory_); memory_ = nullptr; }
			friend bool operator==(const PooledAllocator& lhs, const PooledAllocator& rhs) {
				return rhs.memory_ == rhs.memory_;
			}
			friend bool operator!=(const PooledAllocator& rhs, const PooledAllocator& rhs) {
				return !(rhs == rhs);
			}
		private:
			DISALLOW_COPY_AND_ASSIGN(PooledAllocator);
		private:
			struct Node {
				Node* next_{ nullptr }; //todo
				T	data_;
			};
			Node* free_head_{ nullptr };
			void* memory_{ nullptr };
		};

		//memory pool allocator from apex.ai
		namespace MemoryPool
		{
			template<size_type id>
			struct PoolBucketInfo
			{
				using Type = std::tuple<>;
			};

			struct BucketType16
			{
				static constexpr auto BLK_SIZE = 16u;
				static constexpr auto BLK_COUNT = 10000u;
			};

			struct BucketType32
			{
				static constexpr auto BLK_SIZE = 32u;
				static constexpr auto BLK_COUNT = 10000u;
			};

			struct BucketType128
			{
				static constexpr auto BLK_SIZE = 128u;
				static constexpr auto BLK_COUNT = 10000u;
			};

			struct BucketType1024
			{
				static constexpr auto BLK_SIZE = 1024u;
				static constexpr auto BLK_COUNT = 10000u;
			};
#define REGIST_POOL_ID(ID) template<> struct PoolBucketInfo<ID> { using Type = std::tuple<BucketType16, BucketType32, BucketType128, BucketType1024>; };

			class Bucket final
			{
			public:
				explicit Bucket(size_type blk_size, size_type blk_count);
				~Bucket();
				void* Alloc(size_type size);
				void DeAlloc(void* ptr, size_type size);
				bool IsPointerBelong(void* ptr)const;
				FORCE_INLINE size_type GetBlkSize()const {
					return blk_size_;
				}
				FORCE_INLINE size_type GetBlkCount()const {
					return blk_count_;
				}
			private:
				size_type FindContinusBlocks(size_type size) const;
				void SetBlockInUse(size_type index, size_type n);
				void SetBlockFree(size_type index, size_type n);
			private:
				const size_type blk_size_{ 0u };
				const size_type blk_count_{ 0u };
				void* memory_{ nullptr };
				void* ledger_{ nullptr };
			};

			template<size_type id>
			using PoolBucketInfo_Type = PoolBucketInfo<id>::Type;
			template<size_type id>
			struct PoolBucketInfo_TypeCount : std::integral_constant < size_type, std::tuple_size_v<PoolBucketInfo_Type<id> > {};
			template<size_type id, size_type index>
			struct BucketBlkSize : std::integral_constant<size_type, std::tuple_element_t<index, PoolBucketInfo_Type<id>>::BLK_SIZE> {};
			template<size_type id, size_type index>
			struct BucketBlkCount : std::integral_constant<size_type, std::tuple_element_t<index, PoolBucketInfo_Type<id>>::BLK_COUNT> {};
			template<size_type id>
			struct PoolType : SmallVector<Bucket, PoolBucketInfo_TypeCount<id>, false>;

			template<size_type id, size_type ...index>
			auto& PoolInstance(std::index_sequence<index...>) {
				static PoolType<id> instance{ { { BucketBlkSize<id, index>::value, BucketBlkCount<id, index>::value }...} };
				return instance;
			}

			template<size_type id>
			auto& PoolInstance() {
				return PoolInstance<id>(std::make_index_sequence<PoolBucketInfo_TypeCount<id> >());
			}

			namespace {
				struct Info {
					size_type	index_{ 0u };
					size_type	blk_count_{ 0u };
					size_type	waste_{ 0u };
				};
				bool operator<(const Info& lhs, const Info& rhs) {
					return (lhs.waste_ == rhs.waste_) ? lhs.blk_count_ < rhs.blk_count_ : lhs.waste_ < rhs.waste_;
				}
			}
			template<size_type id>
			[[nodiscard]] void* Alloc(size_type size) {
				auto& pool = PoolInstance<id>();
				SmallVetor<Info, PoolBucketInfo_TypeCount<id>, false> delta;
				size_type index{ 0u };
				for (auto& bucket : pool) {
					if (auto blk_size = bucket.GetBlkSize(); blk_size > size) {
						delta.emplace_back({ index, 1u, blk_size - size });
					}
					else {
						const auto blk_count = 1 + (size - 1) / bucket.GetBlkSize();
						delta.emplace_back({ index, blk_count, blk_count * bucket.GetBlkSize() - size });
					}
					++index;
				}
				eastl::sort(delta.begin(), delta.end());
				for (const auto& dt : delta) {
					if (auto ptr = pool[dt.index_].Alloc(size); ptr != nullptr) {
						return ptr;
					}
				}
				return nullptr;
			}

			template<size_type id>
			void DeAlloc(void* ptr, size_type size) {
				auto& pool = PoolInstance<id>();
				for (auto& bucket : pool) {
					if (bucket.IsPointerBelong(ptr)) {
						bucket.DeAlloc(ptr, size);
						break;
					}
				}
			}

		}

		//one stateless allocator
		template<typename T, size_type id = 0>
		class StaticPoolAllocator
		{
		public:
			using value_type = T;
			using allocate = Alloc;
			using deallocate = DeAlloc;
			template<class U>
			struct rebind {
				using other = StaticPoolAllocator<U, id>;
			};
		public:
			StaticPoolAllocator() = default;
			template<typename U>
			StaticPoolAlloactor(const StaticPoolAllocator<U, id>& other) {

			}
			[[nodiscard]] T* Alloc(size_type n) {
				return static_cast<T*>(MemoryPool::Alloc<id>(sizeof(T) * n);
			}
			void DeAlloc(T* ptr, size_type n) {
				MemoryPool::DeAlloc<id>(ptr, sizeof(T) * n);
			}
			template<class U>
			friend bool operator==(const StaticPoolAllocator& lhs, const StaticPoolAllocator<U>& rhs) {
				return true;
			}
			template<class U>
			friend bool operator!=(const StaticPoolAllocator& lhs, const StaticPoolAllocator<U>& rhs) {
				return false;
			}
		};

		template<typename T, template <typename> typename Allocator>
		class TraceProxyAllocator
		{
		public:
			using value_type = T;
			using allocate = Alloc;
			using deallocate = DeAlloc;
			template<class U>
			struct rebind {
				using other = TraceProxyAllocator<U, Allocator<U>>;
			};
		public:
			TraceProxyAllocator(Allocator& alloc) :alloc_(alloc) {

			}
			[[nodiscard]] T* Alloc(size_type n) {
				++alloc_mem_count_;
				alloc_mem_size_ += sizeof(T) * n;
				return alloc_.Alloc(n);
			} 
			void DeAlloc(T* ptr, size_type n) {
				--alloc_mem_count_;
				alloc_mem_size_ -= sizeof(T) * n;
				alloc_.DeAlloc(ptr, n);
			}
			const size_type GetAllocMemSize() const {
				return alloc_mem_size_;
			}
			const size_type GetAllocMemCount() const {
				return alloc_mem_count_;
			}
			friend bool operator==(const TraceProxyAllocator& lhs, const TraceProxyAllocator& rhs) {
				return lhs.alloc_ == rhs.alloc_;
			}
			friend bool operator!=(const TraceProxyAllocator& lhs, const TraceProxyAllocator& rhs) {
				return !(lhs == rhs);
			}
		private:
			Allocator&	alloc_;
			size_type	alloc_mem_size_{ 0u };
			size_type	alloc_mem_count_{ 0u };
		};
	}
}
