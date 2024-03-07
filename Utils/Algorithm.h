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

        template<typename Traits>
        concept IntrusiveListTraitsConcept = requires{
            typename Traits::ItemType;
        }&& requires (const typename Traits::ItemType* ele) {
                { Traits::GetPrev(ele) }->std::same_as<const typename Traits::ItemType*>;
                { Traits::GetNext(ele) }->std::same_as<const typename Traits::ItemType*>;
        }&& requires (typename Traits::ItemType* ele) {
                { Traits::AccessPrev(ele) }->std::same_as<typename Traits::ItemType*&>;
                { Traits::AccessNext(ele) }->std::same_as<typename Traits::ItemType*&>;
        };

        template<typename T>
        requires std::is_same_v<decltype(std::declval<T>().prev_), decltype(std::declval<T>().next_)> //check whether has member prev_ & next_
        struct DefaultIntrusiveListTraits
        {
            using ItemType = T;
            using Pointer = ItemType*;
            static const Pointer GetPrev(const Pointer ptr) {
                return ptr->prev_;
            }
            static const Pointer GetNext(const Pointer ptr) {
                return ptr->next_;
            }
            static Pointer& AccessPrev(Pointer ptr) {
                return ptr->prev_;
            }
            static Pointer& AccessNext(Pointer ptr) {
                return ptr->next_;
            }
        };
        
        template<IntrusiveListTraitsConcept Traits, typename ItemType>
        class InstrusiveLinkedListIterator
        {
        public:
            using value_type = ItemType;
            using pointer = ItemType*;
            using reference = value_type&;
            using iterator_category = std::bidirectional_iterator_tag;
        public:
            InstrusiveLinkedListIterator() = default;
            explicit InstrusiveLinkedListIterator(pointer ptr) :item_ptr_(ptr) {}
            template<typename U>
            explicit InstrusiveLinkedListIterator(const InstrusiveLinkedListIterator<Traits, U>& other) : item_ptr_(other.item_ptr) {}
            reference operator*() { return *iterm_ptr_; }
            pointer operator->() { return iterm_ptr_; }
            auto& operator++() {
                assert(item_ptr_ != nullptr);
                if constexpr(requires std::is_const<value_type>) {
                    item_ptr_ = Traits::GetNext(item_ptr_);
                }
                else
                {
                    item_ptr_ = Traits::AccessNext(item_ptr);
                }
                return *this;
            }
            auto operator++(int) {
                auto temp = *this;
                ++*this;
                return temp;
            }
            auto& operator--() {
                assert(item_ptr_ != nullptr);
                if constexpr (requires std::is_const<value_type>) {
                    item_ptr_ = Traits::GetPrev(item_ptr_);
                }
                else
                {
                    item_ptr_ = Traits::AccessPrev(item_ptr_);
                }
                return *this;
            }
            auto operator--(int) {
                auto temp = *this;
                --*this;
                return temp;
            }
            bool operator==(const auto& rhs)const {
                return item_ptr_ == rhs.item_ptr_;
            }
            bool operator!=(const auto& rhs)const {
                return !(*this == rhs);
            }
        private:
            pointer item_ptr_{ nullptr };
        };
        
        //this core merged from vk_mem_allocator.h
        template<IntrusiveListTraitsConcept Traits, template<typename, typename> class IteratorType = InstrusiveLinkedListIterator>
        class IntrusiveLinkedList
        {
        public:
            using ItemType = Traits::ItemType;
            using Iterator = IteratorType<Traits, ItemType>;
            using ConstIterator = IteratorType<Traits, const IteratorType>;
        public:
            IntrusiveLinkedList() = default;
            ~IntrusiveLinkedList() { assert(empty()); }
            /*stl interface*/
            Iterator begin() {
                return { Front(); }
            }
            ConstIterator cbegin()const {
                return { Front() };
            }
            Iterator end() {
                return { Back() };
            }
            ConstIterator cend()const {
                return { Back() };
            }
            ItemType* front() {
                return front_;
            }
            ItemType* back() {
                return back_;
            }
            void push_back(ItemType* item) {
                if (empty()) {
                    front_ = item;
                    back_ = item;
                    Traits::AccessPrev(item) = nullptr;//
                }
                else {
                    Traits::AccessNext(back_) = item;
                    Traits::AccessPrev(item) = std::exchange(back_, item);
                }
                Traits::AccessNext(item) = nullptr;//
                ++size_;
            }
            void push_front(ItemType* item) {
                if (empty()) {
                    front_ = item;
                    back_ = item;
                    Traits::AccessNext(item) = nullptr;//
                }
                else {
                    Traits::AccessPrev(front_) = item;
                    Traits::AccessNext(item) = std::exchange(font_, item);
                }
                Traits::AccessPrev(item) = nullptr; //
                ++size_;
            }
            ItemType* pop_back() {
                if (empty()) {
                    return nullptr;
                }
                else
                {
                    auto* const prev_item = Traits::GetPrev(back_);
                    if (prev_item != nullptr) {
                        Traits::AccessNext(prev_item) = nullptr;
                    }
                    auto* output = std::exchange(back_, prev_item);
                    Traits::AccessPrev(output) = nullptr;
                    Traits::AccessNext(output) = nullptr;
                    --size_;
                    return output;
                }
            }
            ItemType* pop_front() {
                if (empty()) {
                    return nullptr;
                }
                else
                {
                    auto* const next_item = Trait::GetNext(front_);
                    if (nex_item != nullptr) {
                        Traits::AccessPrev(next_item) = nullptr;
                    }
                    auto* output = std::exchange(front_, next_item);
                    Traits::AccessPrev(output) = nullptr;
                    Traits::AccessNext(output) = nullptr;
                    --size_;
                    return output;
                }
            }
            void insert_before(ItemType* curr, ItemType* new_item) {
                if (curr == nullptr || curr == front_) {
                    return push_front(new_item);
                }
                Traits::AccessNext(new_item) = curr;
                Traits::AccessNext(Traits::AccessPrev(curr)) = new_item;
                Traits::AccessPrev(new_item) = std::exchange(Traits::AccessPrev(curr), new_item);
            }
            void insert_after(ItemType* curr, ItemType* new_item) {
                if (curr == nullptr || curr == back_) {
                    return push_back(new_item);
                }
                Traits::AccessPrev(new_item) = curr;
                Traits::AccessPrev(Traits::AccessNext(curr)) = new_item;
                Traits::AccessPrev(new_item) = std::exchange(Traits::AccessPrev(curr), new_item);
            }
            void remove(ItemType* item) {
                if (item == front_) {
                    front_ = Traits::GetNext(item);
                }
                else
                {
                    Traits::AccessNext(Traits::AccessPrev(item)) = Traits::GetNext(item);
                }
                if (item == back_) {
                    back_ = Traits::GetPrev(item);
                }
                else
                {
                    Traits::AccessPrev(Traits::AccessNext(item)) = Traits::GetPrev(item);
                }
                Traits::AccessPrev(item) = nullptr;
                Traits::AccessNext(item) = nullptr;
                --size_;
            }
            Iterator insert(Iterator pos, ItemType* item) {
                insert_after(*pos, item);
                return Iterator(item);
            }
            Iterator erase(Iterator pos) {
                auto* next = Traits::AccessNext(item);
                remove(*pos);
                //todo check back_ or empty
                return Iterator(next);
            }
            void clear() {
                if (!empty()) {
                    auto* item = back_;
                    while (item != nullptr) {
                        auto* const prev_item = Traits::AccessPrev(item);
                        Traits::AccessNext(item) = nullptr;
                        Traits::AccessPrev(item) = nullptr;
                        //todo whether delete it
                        item = prev_item;
                    }
                    *this = {};
                }
            }
            bool empty()const { return !size_; }
            size_type size() const { return size_; }
        private:
            DISALLOW_COPY_AND_ASSIGN(IntrusiveLinkedList);
        private:
            size_type   size_{ 0ull };
            ItemType*   front_{ nullptr };
            ItemType*   back_{ nullptr };

        };
    }
}

#include "Algorithm.inl"
