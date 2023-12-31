#pragma once
#include <version>
#include "Utils/SimpleJobSystem.h"

#if defined(USE_STD_COROUTINE) && defined(__cpp_lib_coroutine) && !defined(LLVM_COROUTINES) && \
    (!defined(_LIBCPP_VERSION) || __cplusplus > 201703L)
#include <coroutine>
namespace impl = std;
//#define USE_SYMMETRIC_TRANSFER
#else
#include <experimental/coroutine>
namespace impl = std::experimental;
#endif 


namespace Shard
{
    namespace Utils
    {
        template<typename RetVal = void> class Coro;
        //awaitables for co_wait and co_yield, and final awaiting
        template<typename RetVal, typename... Ts> class AwaitTuple;
        template<typename RetVal> class AwaitResumeOn;
        template<typename U> class YieldAwaiter;
        template<typename U> class FinalAwaiter;
        class CoroPromiseBase;
        template<typename RetVal = void> class CoroPromise;

        /*
        * The void-returning version of await_suspend() unconditionally transfers
        * execution back to the caller/resumer of the coroutine when the call to
        * await_suspend() returns, whereas the bool-returning version allows the
        * awaiter object to conditionally resume the coroutine immediately without
        * returning to the caller/resumer.
        */
        template<typename RetVal, typename... Ts>
        class AwaitTuple final
        {
        public:
            using CoroHandle = impl::coroutine_handle<CoroPromise<RetVal>>;
            /** 
             * \param tuple: which convert to awaitable
             */
            AwaitTuple(std::tuple<Ts&&...> tuple);
            constexpr bool await_ready() noexcept;
            void await_suspend(CoroHandle handle)noexcept;
            //return result from coroutine
            decltype(auto) await_resume()noexcept;
        private:
            std::tuple<Ts&...>  tuple_;
            size_type   number_{ 0u };
        };

        template<typename RetVal>
        class AwaitResumeOn final: impl::suspend_always
        {
        public:
            using CoroHandle = impl::coroutine_handle<CoroPromise<RetVal>>;
            AwaitResumeOn();
            constexpr bool await_ready() noexcept override;
            void await_resume(CoroHandle h) noexcept override;
        };

        template<typename U>
        class YieldAwaiter final
        {
        public:
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(impl::coroutine_handle<CoroPromise<U> > h) noexcept;
            constexpr void await_resume() const noexcept {}
        };

        template<>
        class YieldAwaiter<void> final
        {
        public:
            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(impl::coroutine_handle<CoroPromise<void> > h) noexcept;
            constexpr void await_resume()noexcept {}
        };

        template<typename U>
        class FinalAwaiter final
        {
        public:
            constexpr bool await_ready() const noexcept { return false; }
            //return false indicate that the coroutine should be immediately resumed and continue execution WITH NO STACK CONSUME
            bool await_suspend(impl::coroutine_handle<CoroPromise<U> > h) noexcept;
            constexpr void await_resume()noexcept {}
        };

        template<>
        class FinalAwaiter<void> final
        {
        public:
            constexpr bool await_ready() const noexcept { return false; }
            bool await_suspend(impl::coroutine_handle<CoroPromise<void> > h) noexcept;
            constexpr void await_resume()noexcept {}
        };

        //Promise interface specifies methods for customising the behaviour of the coroutine itself. 
        class CoroPromiseBase : public JobEntry 
        {
        public:
            explicit CoroPromiseBase(impl::coroutine_handle<> coro) noexcept : JobEntry(), coro_(coro) {}
            virtual ~CoroPromiseBase() = default;
            constexpr bool IsCoro() const final {
                return true;
            }
            bool IsParentCoro() const {
                return is_parent_coro_;
            }
            bool IsSelfDeConstructEnabled() const {
                return is_self_deconstruct_;
            }
            void EnableSelfDeConstruct() {
                is_self_deconstruct_ = true;
            }
            void UnableSelfDeConstruct() {
                is_self_deconstruct_ = false;
            }
            void operator()(void)override {
                resume();
                NotifyFinished();
            }
            void Release()override {

            }
            virtual void resume() = 0;
            void unhandled_exception() {
                auto except = std::current_exception();
                if (except != nullptr) {
                    LOG(ERROR) << "coro task execute failded with exception";
                    std::rethrow_exception(except);
                }
            }
            /*If the coroutine suspends at the initial suspend point then it can be later resumed 
            * or destroyed at a time of your choosing by calling resume() or destroy() on the 
            * coroutine¡¯s coroutine_handle.The result of the co_await promise.initial_suspend()
            * expression is discarded so implementations should generally return void from the 
            * await_resume() method of the awaiter. ALWAYS SUSPEND AND USE JOB SYSTEM TO EXECUTE IT
            */
            impl::suspend_always initial_suspend() noexcept { return {}; }
            /**  
            *https://lewissbaker.github.io/2018/09/05/understanding-the-promise-type say:
            *the size passed to operator new is not sizeof(P) but is rather the size of the entire coroutine frame.
            * so, children class use the base class overload new is ok?
            */
            template<typename ...ARGS>
            void* operator new(std::size_t size, ARGS&&... args) {
                auto* allocator = TLSScalablePoolAllocatorInstance<uint8_t, POOL_JOBSYSTEM_ID>();
                const auto alloc_size = size + sizeof(std::uintptr_t);
                auto* ptr = allocator->allocate(alloc_size);
                *reinterpret_cast<std::uintptr_t*>(ptr) = (std::uintptr_t)allocator; //record the allocator of this thread
                return ptr + sizeof(std::uintptr_t);
            }
            void operator delete(void* ptr, std::size_t size) {
                auto raw_ptr = (std::uintptr_t)(ptr)-sizeof(std::uintptr_t);
                auto* allocator = reinterpret_cast<ScalablePoolAllocator<uint8_t>*>(raw_ptr); //will release in another thread...
                allocator->deallocate((uint8_t*)raw_ptr, size + sizeof(std::uintptr_t));
            }
        protected:
            impl::coroutine_handle<>    coro_;
            bool    is_parent_coro_{ SimpleJobSystem::Instance().GetCurrentJob() == nullptr || SimpleJobSystem::Instance().GetCurrentJob()->IsCoro() };
            bool    is_self_deconstruct_{ false };
        };
        
        template<typename RetVal>
        class  CoroPromise final : public CoroPromiseBase
        {
        public:
            using CoroHandle = impl::coroutine_handle<CoroPromise<RetVal> >;
            CoroPromise();
            JobEntry::JobDeAllocator GetDeAllocator() const;
            void return_value(RetVal val);
            static Coro<RetVal> get_return_object_on_allocation_failure();
            Coro<RetVal> get_return_object() noexcept;
            void resume()final;
            /**
             * \brief Called by co_await to create an awaitable for coroutines, Functions, vectors, or tuples thereof.
             * \param[in] func The coroutine, Function or vector to await.
             * \returns the awaitable for this parameter type of the co_await operator.
             */
            template<typename U>
            AwaitTuple<RetVal, U> await_transform(U&& func)noexcept;
            template<typename... Ts>
            AwaitTuple<RetVal, Ts...> await_transform(std::tuple<Ts...>&& tuple) noexcept;
            template<typename... Ts>
            AwaitTuple<RetVal, Ts...> await_transform(std::tuple<Ts...>& tuple) noexcept;
            YieldAwaiter<RetVal> yield_value(RetVal val);
            /*
            * Once execution exits the user-defined part of the coroutine body and the result 
            * has been captured via a call to return_void(), return_value() or unhandled_exception()
            * and any local variables have been destructed, the coroutine has an opportunity to 
            * execute some additional logic before execution is returned back to the caller/resumer.
            */
            FinalAwaiter<RetVal> final_suspend() noexcept { return {}; }
        private:
            friend class Coro<RetVal>;
            friend class YieldAwaiter<RetVal>;
            friend class FinalAwaiter<RetVal>;
            //de-coroutine for function
            std::shared_ptr<std::pair<bool, RetVal> >   func_value_;
            std::pair<bool, RetVal> value_;
        };

        template<>
        class CoroPromise<void> final : public CoroPromiseBase 
        {
        public:
            using CoroHandle = impl::coroutine_handle<CoroPromise<void> >;
            CoroPromise();
            JobEntry::JobDeAllocator GetDeAllocator()const final;
            void return_void() noexcept {}
            void resume() final;
            template<typename U>
            AwaitTuple<void, U> await_transform(U&& func) noexcept;
            template<typename ...T>
            AwaitTuple<void, T...> await_transform(std::tuple<T...>&& tuple) noexcept;
            template<typename ...T>
            AwaitTuple<void, T...> await_transform(std::tuple<T...>& tuple) noexcept;
            YieldAwaiter<void> yield_value() noexcept { return {}; }
            FinalAwaiter<void> final_suspend() noexcept { return {}; }
            static Coro<void> get_return_object_on_allocation_failure();
            Coro<void> get_return_object() noexcept;
        private:
            friend class Coro<void>;
            friend class YieldAwaiter<void>;
            friend class FinalAwaiter<void>;
            std::shared_ptr<std::pair<bool, bool> >    func_value_; //todo 
        };

        class CoroBase
        {
        protected:
            CoroPromiseBase* promise_{ nullptr };
        public:
            CoroBase() = default;
            explicit CoroBase(CoroPromiseBase* promise) :promise_(promise) {}
            bool resume() noexcept;
            CoroPromiseBase* promise() noexcept { return promise_; }
            virtual ~CoroBase() = default;
        };

        //class coroutine return object 
        template<typename RetVal = void>
        class [[nodiscard]] Coro final: public CoroBase
        {
        public:
            using promise_type = CoroPromise<RetVal>;
            //https://en.cppreference.com/w/cpp/coroutine/coroutine_traits
            using PromiseType = promise_type;//for compiler to def promise type
            using CoroHandle = impl::coroutine_handle<PromiseType>;
            Coro() = default;
            explicit Coro(CoroHandle handle) noexcept;
            Coro(Coro&& rhs)noexcept;
            Coro& operator=(Coro&& rhs)noexcept;
            ~Coro();
            bool IsReady()const;
            RetVal GetValue();
            CoroHandle GetHandle();
            bool IsParentCoro()const;
        private:
            void EnsureValid()const;
        private:
            CoroHandle        handle_;
        };

        template<>
        class [[nodiscard]] Coro<void> final : public CoroBase
        {
        public:
            using promise_type = CoroPromise<void>;
            using PromiseType = promise_type;
            using CoroHandle = impl::coroutine_handle<PromiseType>;
            Coro() = default;
            explicit Coro(CoroHandle handle) noexcept;
            Coro(Coro&& rhs)noexcept;
            Coro& operator=(Coro&& rhs)noexcept;
            ~Coro();
            bool IsParentCoro()const;
        private:
            void EnsureValid()const;
        private:
            CoroHandle        handle_;
        };

        template<typename T>
        requires std::is_base_of_v<CoroBase, std::decay_t<T>>
        decltype(auto) Schedule(T&& coro, JobEntry* parent=SimpleJobSystem::Instance().GetCurrentJob());

        template<typename T>
        requires std::is_base_of_v<CoroBase, std::decay_t<T>>
        void Continuation(T&& coro)noexcept;

        //functions to help co_wait tuple
        namespace AwaitTupleHelper
        {
            template<typename... Ts>
            decltype(auto) Parallel(Ts&&... args); //return value for co_await

            template<typename... Ts>
            //args order is in reverse of executing order
            decltype(auto) Sequential(Ts&&... args); //return value for co_await
        }

    }
}

#include "Utils/SimpleCoro.inl"
