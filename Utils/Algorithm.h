#pragma once
#include "Utils/CommonUtils.h"

namespace Shard
{
    namespace Utils
    {
        //https://www.lenshood.dev/2021/04/19/lock-free-ring-buffer/#ring-buffer
        /**
         * a ring buffer suitable for MPMC  
         * T: element type
         * null: object of type T, null object to fix ABA problem
         * ContainerType: backend storage type with method Set and Get
         */
        template <class T, T null, template<typename> class ContainerType>
        class TRingBuffer 
        {
        public:
            using StorageClass = ContainerType<T>;
            using Handle = StorageClass::size_type;
            using ElementType = T;
            TRingBuffer() = default;
            explicit TRingBuffer(size_type capacity);
            TRingBuffer(TRingBuffer&& other);//todo a atomic not moveable
            void operator=(TRingBuffer&& other);
            virtual bool TryPoll(ElementType& el);
            virtual bool TryOffer(const ElementType& el);
            size_type GetCapacity()const {
                return capacity_;
            }
            virtual ~TRingBuffer() = default;
        private:
            StorageClass    storage_;
            std::atomic<Handle> head_{ 0u };
            std::atomic<Handle> tail_{ 0u };
            const size_type capacity_{ 0u };
        };

        //lock-free treiber stack https://en.wikipedia.org/wiki/Treiber_stack
        template<typename T, template<typename> class ContainerType>
        class TTreiberStack
        {
            struct alignas(128) TopNode //align with cache line
            {
                uint32_t    index_{ 0u };
                uint32_t    version_{ 0u }; //use to avoid ABA
            };
            std::atomic<TopNode>    top_node_;
            ContainerType<T>& node_container_;
        public:
            using ElementType = T;
            explicit TTreiberStack(ContainerType<ElementType>& nodes) :node_container_(nodes) {

            }
            void Push(ElementType&& ele) {
                do { //todo overflow
                    auto local_top_node = top_node_.load(std::memory_order::relaxed);
                    node_container_[local_top_node.index_ + 1] = ele;
                    TopNode new_top_node{ local_top_node.index_ + 1, local_top_node.version_ + 1 };
                    if (top_node_.compare_and_exchange_weak(local_top_node, new_top_node, std::memory_order::memory_order_seq_cst, std::memory_order::relaxed)) {
                        break;
                    }
                } while (true);

            }
            ElementType Poll() {
                ElementType ele{};
                do {
                    auto local_top_node = top_node_.load(std::memory_order::relaxed);
                    if (local_top_node.index_ < 0u) {
                        break;
                    }
                    ele = node_container_[local_top_node.index_];
                    TopNode new_top_node{ local_top_node.index_ - 1, local_top_node.version_ + 1 };
                    if (top_node_.compare_and_exchange_weak(local_top_node, new_top_node, std::memory_order::memory_order_seq_cst, std::memory_order::relaxed))
                    {
                        break;
                    }
                } while (true);
                return ele;
            }
        };
    }
}

#include "Algorithm.inl"
