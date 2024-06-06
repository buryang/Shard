#pragma once

namespace Shard
{
    namespace Utils
    {
        template<typename Tag, uint32_t ID=24, uint32_t GEN=8> 
        class Handle 
        {
        public:
            explicit Handle(uint32_t index = 0u, uint8_t generation = 0u)noexcept : index_(index), generation_(generation) {}
            bool IsValid()const { return generation_ != 0; }
            uint32_t Index()const { return index_; }
            uint32_t Generation()const { return generation_; }
            explicit operator uint32_t()const { return Index(); }
            auto operator++(int) {
                Handle temp(*this);
                ++(*this);
                return temp;
            }
            auto& operator++() { 
                ++generation_;
                return *this;
            }
            //ambiguity with ++
            bool operator==(Handle other)const { return Index() == other.Index() && Generation() == other.Generation(); }
            bool operator!=(Handle other)const { return !(*this == other); }
            bool operator<=(Handle other)const { return Index() <= other.Index(); }
            bool operator>=(Handle other)const { return Index() >= other.Index(); }
            bool operator<(Handle other)const { return Index() < other.Index(); }
            bool operator>(Handle other)const { return Index() > other.Index(); }
            virtual ~Handle() = default;
        protected:
            uint32_t    index_ : ID{ 0u }; //element cout less than 2^24
            uint32_t    generation_ : GEN{ 0u };
        };

        template<typename T>
        class HandleManager 
        {
        public:
            using Type = T;
            using Handle = Handle<Type>;
            Handle Alloc() {
                auto index = GetFreeHandle();
                if (index != -1) {
                    generation_[index] = 1u;
                    return { index, 1u };
                }
                generation_.push_back(1u);
                return { generation_.size() - 1, 1u };
            }
            void Free(Handle handle) {
                generation_[handle.Index()]++;
                free_list_.push_back(handle.Index());
            }
            void Clear() {

            }
            bool IsValid(Handle handle) {
                return handle.IsValid() && generation_[uint32_t(handle)] == handle.Generation();
            }
            virtual ~HandleManager() = default;
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
            Vector<uint8_t> generation_;
            Vector<uint32_t>    free_list_;
        };
        
        //use template<typename...> report error
        template<typename, template<typename>typename...>
        class ResourceManager;

        //todo support multithread
        //rewrite it next day with sparse set
        template<typename T>
        class ResourceManager<T> : public HandleManager<T>
        {
        public:
            template<typename U>
            concept RequiredType = std::is_base_of_v<T, U>;
            using Type = HandleManager<T>::Type;
            class ResourceHandle : public HandleManager<T>::Handle
            {
            public:
                using BaseClass = HandleManager<T>::Handle;
                ResourceHandle(BaseClass handle, ResourceManager& manager) :BaseClass(handle), manager_(manager) {}
                operator BaseClass()const { return { index_, generation_ }; }
                Type& operator*() {
                    return *manager_.Get(*this);
                }
                Type* operator->() {
                    return manager_.Get(*this);
                }
            private:
                ResourceManager& manager_;
            };
            using Handle = ResourceHandle;
            ResourceManager() = default;

            template<RequiredType U, typename... Args>
            Handle Alloc(Args&&... args) {
                auto handle = HandleManager<T>::Alloc();
                auto* ptr = new U(std::forward<Args>(args)...);
                assert(ptr != nullptr);
                if (handle.Index() < storage_.size()) {
                    storage_[handle.Index()] = (Type*)(ptr);
                }
                else
                {
                    storage_.emplace_back((Type*)(ptr));
                }
                return { handle, *this };
            }

            void Free(Handle handle) override{
                HandleManager<T>::Free(handle);
                delete storage_[handle];
                storage_[handle] = nullptr;
            }
            void Clear() {
                HandleManager<T>::Clear();
                for (auto* ptr : storage_) {
                    delete ptr;
                }
            }
            template<RequiredType U>
            U* Get(Handle handle) {
                if (IsValid(handle)) {
                    return storage_[handle];
                }
                return nullptr;
            }
        private:
            Vector<T*>  storage_;
        };

        template<typename T, template<typename>typename Allocator>
        class ResourceManager<T, Allocator> : public HandleManager<T>
        {
        public:
            using Type = HandleManager<T>::Type;
            class ResourceHandle : public HandleManager<T>::Handle
            {
            public:
                using BaseClass = HandleManager<T>::Handle;
                ResourceHandle(BaseClass handle, ResourceManager& manager) :BaseClass(handle), manager_(manager) {}
                operator BaseClass()const { return { index_, generation_ }; }
                Type& operator*() {
                    return *manager_.Get(*this);
                }
                Type* operator->() {
                    return manager_.Get(*this);
                }
            private:
                ResourceManager& manager_;
            };
            using Handle = ResourceHandle;
            using AllocatorType = Allocator<T>;
            explicit ResourceManager(AllocatorType& alloc) : storage_(alloc) {}

            template<typename... Args>
            Handle Alloc(Args&&... args) {
                auto handle = HandleManager<T>::Alloc();
                if (handle.Index() > storage_.size()) {
                    storage_.emplace_back(Type{ std::forward<Args>(args)... });
                }
                else{
                    new(&storage_[handle.Index()])Type(std::forward<Args>(args)...);
                }
                return { handle, *this };
            }

            void Free(Handle handle) override {
                HandleManager<T>::Free(handle);
                storage_[handle]->~Type();
            }
            void Clear() {
                HandleManager<T>::Clear();
                for (auto* ptr : storage) {
                    ptr->~Type();
    
                }
            }
            Type* Get(Handle handle) {
                if (IsValid(handle)) {
                    return storage_[handle];
                }
                return nullptr;
            }
        private:
            Vector<T, AllocatorType>  storage_;
        };
    }
}

