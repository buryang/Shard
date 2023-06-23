#pragma once
#include "Utils/CommonUtils.h"
#include "folly/memory/arena.h"
#include <>

namespace Shard
{
	namespace Utils
	{
		class MINIT_API LinearAllocator
		{
		public:
			LinearAllocator() = default;
			[[nodiscard]] void* Alloc(std::size_t size);
			template <typename T, typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc(sizeof(T));
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(void* ptr, std::size_t size);
			~LinearAllocator();
		public: 
			//field for allocator_traits

		private:
			void* memory_{ nullptr };
		};

		//realize allocator from book game-engine-architecture
		class MINIT_API StackAllocator
		{
		public:
			explicit StackAllocator(size_t stack_size);
			[[nodiscard]] void* Alloc(std::size_t size);
			template <typename T, typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc(sizeof(T));
				return new(ptr)T(std::forward<Args>(args)...);
			}
			void DeAlloc(void* ptr, std::size_t size);
			~StackAllocator();
		public:
			//field for allocator_traits
		private:
			std::size_t capacity_{ 0u };
			std::size_t	offset_{ 0u };
			void* memory_{ nullptr };
		};

		template<typename T>
		class PooledAllocator
		{
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
					tail->next_ = tail_ + 1;
				}
				tail->next_ = nullptr;
			}
			//todo lock
			[[nodiscard]] T* Alloc() {
				auto* next = free_head_->next_;
				PCHECK(next != nullptr) << "memory pool for type is not enough";
				std::swap(next, free_head_);
				return &(next->data_);
			}
			template <typename... Args>
			[[nodiscard]] T* AllocNoDestruct(Args&&... args) {
				auto ptr = Alloc();
				return new(ptr)T(std::forward<Args>(args)...);
			}
			//todo lock
			void DeAlloc(T* ptr) {
				auto* node = reintepret_cast<Node*>(static_cast<uintptr_t>(ptr) - sizeof(Node*));
				node->next_ = free_;
				std::swap(node, free_head_);
			}
			~PooledAllocator() { ::operator delete memory_; memory_ = nullptr; }
		public:
			//fields for allocator_traits
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
