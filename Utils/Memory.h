#pragma once
#include "Utils/CommonUtils.h"
#include "folly/memory/arena.h"

namespace Shard
{
	namespace Utils
	{
		template<class T>
		class LinearAllocator
		{
		public:
			//field for allocator_traits
			using value_type = T;
			using allocate = Alloc;
			using deallocate = DeAlloc;
		public:
			LinearAllocator(size_type capacity) :capacity_(capacity) {
				memory_ = ::operator new(capacity_);
			}
			[[nodiscard]] T* Alloc(size_type n = 1u) {
				auto ptr = static_cast<T*>(memory_ + offset_);
				offset_ += n * sizeof(T);
				if (offset_ >= capacity_) {
					return nullptr;
				}
				return ptr;
			}
			template <typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc();
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(T* ptr, size_type n) {
				LOG(ERROR) << "linearallocator has no dealloc implementation";
			}
			~LinearAllocator() {
				::operator delete(memory_);
			}
			friend bool operator==(const LinearAllocator& lhs, const LinearAllocator& rhs) {
				return lhs.memory_ != rhs.memory_;
			}
			friend bool operator!=(const LinearAllocator& lhs, const LinearAllocator& rhs) {
				return !(lhs == rhs);
			}
		private:
			DISALLOW_COPY_AND_ASSIGN(LinearAllocator);
		private:
			size_type	capacity_{ 0u };
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
		public:
			[[nodiscard]] T* Alloc(size_type n = 1u) {
				auto ptr = static_cast<T*>(memory_ + offset_);
				offset_ += n * sizeof(T);
				if (offset_ >= capacity_) {
					return nullptr;
				}
				return ptr;
			}
			template <typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc();
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(T* ptr, size_type n) {
				std::destroy(ptr, ptr + n);
				offset_ = std::uintptr_t(memory_) - std::uintptr_t(ptr);
			}
			friend bool operator==(const StackAllocator& lhs, const StackAllocator& rhs) {
				return lhs.memory_ == rhs.memory_;
			}
			friend bool operator!=(const StackAllocator& lhs, const StackAllocator& rhs) {
				return !(lhs == rhs);
			}
		private:
			DISALLOW_COPY_AND_ASSIGN(StackAllocator);
		private:
			size_type	capactity_{ 0u };
			size_type	offset_{ 0u };
			void* memory_{ nullptr };
		};

		template<class T>
		class DoubleEndStackAllocator
		{
			//todo
		};

		template<typename T>
		class PooledAllocator
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
		private:
			struct Node {
				Node*	next_{ nullptr }; //todo
				T	data_;
			};
			Node* free_head_{ nullptr };
			void*	memory_{ nullptr };
		};
	}
}
