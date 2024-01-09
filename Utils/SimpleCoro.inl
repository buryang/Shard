#include "SimpleCoro.h"
//https://github.com/hlavacs/ViennaGameJobSystem/blob/master/include/VGJSCoro.h
//https://lewissbaker.github.io/2017/09/25/coroutine-theory

#define AWAIT_SUSPEND_PROC()\
auto& promise = h.promise();\
auto parent = promise.parent_;\
do\
{\
    if (nullptr != parent) {\
        if (promise.IsParentCoro()) {\
            parent->DecRef();\
            if (parent->ref_count_.load() == 1) {\
                SimpleJobSystem::Instance().Execute(parent);\
            }\
        }\
        else\
        {\
            SimpleJobSystem::Instance().ChildFinished(parent);\
        }\
    }\
}while(0);

namespace Shard
{
    namespace Utils
    {
        bool CoroBase::resume() noexcept
        {
            promise_->resume();
            return true;
        }

        template<typename RetVal>
        inline Coro<RetVal>::Coro(CoroHandle handle) noexcept :CoroBase(&handle.promise()), handle_(handle)
        {
        }

        template<typename RetVal>
        inline Coro<RetVal>::Coro(Coro&& rhs) noexcept : CoroBase(rhs.promise_), handle_(std::exchange(rhs.handle_, {}))
        {
            assert(this != &rhs);
        }

        template<typename RetVal>
        inline Coro<RetVal>& Coro<RetVal>::operator=(Coro&& rhs) noexcept
        {
            assert(this != &rhs);
            promise_ = std::exchange(rhs.promise_, {});
            handle_ = std::exchange(rhs.handle_, {});
            return *this;
        }

        template<typename RetVal>
        inline Coro<RetVal>::~Coro()
        {
            if (handle_ && IsParentCoro()) {
                if (handle_.promise().IsSelfDeConstructEnabled()) return; 
                handle_.destroy();
            }
        }

        template<typename RetVal>
        inline bool Coro<RetVal>::IsReady()const
        {
            EnsureValid();
            if (!IsParentCoro()) {
                return handle_.promise().func_value_->first;
            }
            return handle_.promise().value_.first;
        }

        template<typename RetVal>
        inline RetVal Coro<RetVal>::GetValue()
        {
            EnsureValid();
            assert(IsReady());
            if (!IsParentCoro()) {
                return handle_.promise().func_value_->second;
            }
            return handle_.promise().value_.second;
        }

        template<typename RetVal>
        inline Coro<RetVal>::CoroHandle Coro<RetVal>::GetHandle()
        {
            return handle_;
        }
        
        template<typename RetVal>
        inline bool Coro<RetVal>::IsParentCoro() const
        {
            return handle_.promise().is_parent_coro_;
        }

        template<typename RetVal>
        inline void Coro<RetVal>::EnsureValid() const
        {
            if (handle_ == nullptr) {
                LOG(ERROR) << "invalid coroutine handle";
            }
        }

        inline Coro<void>::Coro(CoroHandle handle) noexcept : CoroBase(&handle.promise()), handle_(handle)
        {
        }


        inline Coro<void>::Coro(Coro&& rhs) noexcept : CoroBase(rhs.promise_), handle_(std::exchange(rhs.handle_, {}))
        {
        }

        bool Coro<void>::IsParentCoro()const
        {
            return handle_.promise().is_parent_coro_;
        }

        inline Coro<void>::~Coro()
        {
            if (handle_ && IsParentCoro()) {
                if (handle_.promise().IsSelfDeConstructEnabled()) return;
                handle_.destroy();
            }
        }

        template<typename RetVal, typename ...Ts>
        inline AwaitTuple<RetVal, Ts...>::AwaitTuple(std::tuple<Ts&&...> tuple) :tuple_(std::forward<std::tuple<Ts&&...> >(tuple))
        {

        }

        namespace {

            template<typename>
            constexpr auto is_vector = false;
            template<typename T>
            constexpr auto is_vector<Vector<T> > = true;
            //Note that templates cannot be declared in a function
            template<typename U>
            std::size_t SizeImpl(U& children)
            {
                if constexpr (is_vector<std::decay_t<U> >) {
                    return children.size();
                }

                return 1; //when chidlren is function/coroutine
            }

            template<typename T>
            decltype(auto) GetValueImpl(T& t)
            {
                return std::make_tuple();//for Coro<void>
            }

            template<typename T> 
            requires (!std::is_void_v<T>)
            decltype(auto) GetValueImpl(Coro<T>& coro)
            {
                return std::make_tuple(coro.GetValue());
            }

            template<typename T>
            requires (!std::is_void_v<T>)
            decltype(auto) GetValueImpl(Vector<Coro<T>>& coros)
            {
                Vector<T> ret{ coros.size() };
                for (auto& coro : coros) { ret.emplace_back(coro.GetValue()); }
                return std::make_tuple(std::move(ret));
            }
        }

        template<typename RetVal, typename ...Ts>
        inline constexpr bool AwaitTuple<RetVal, Ts...>::await_ready() noexcept
        {
            auto f = [&, this]<std::size_t... index>(std::index_sequence<index...>) {
                number_ = (SizeImpl(std::get<index>(tuple_)) + ... + 0u);
            };
            f(std::make_index_sequence<sizeof...(Ts)>{});

            return !number_;
        }

        template<typename RetVal, typename ...Ts>
        inline void AwaitTuple<RetVal, Ts...>::await_suspend(AwaitTuple::CoroHandle handle) noexcept
        {
            //single tuple element call
            const auto g = [&, this]<std::size_t index>() {
                using tt = decltype(tuple_);
                using T = decltype(std::get<index>(std::forward<tt>(tuple_)));
                decltype(auto) children = std::forward<T>(std::get<index>(std::forward<tt>(tuple_)));

                //todo schedule
                Schedule(children, &handle.promise());
            };

            const auto f = [&, this]<std::size_t... index>(std::index_sequence<index...>) {
                (g.template operator() < index > (), ...);
            };

            f(std::make_index_sequence<sizeof...(Ts)>{});
            return;
        }

        template<typename RetVal, typename ...Ts>
        inline decltype(auto) AwaitTuple<RetVal, Ts...>::await_resume() noexcept
        {
            auto f = [&, this]<typename ...Us>(Us&... args) {
                return std::tuple_cat(GetValueImpl(args)...);
            };
            auto ret = std::apply(f, tuple_);
            if constexpr (std::tuple_size_v<decltype(ret) > == 0u)
            {
                return;
            }
            else if constexpr (std::tuple_size_v<decltype(ret) > == 1u)
            {
                return std::get<0>(ret);
            }
            else //must keep, for compile
            {
                return ret;
            }
        }

        template<typename RetVal>
        inline AwaitResumeOn<RetVal>::AwaitResumeOn()
        {
        }

        template<typename RetVal>
        inline constexpr bool AwaitResumeOn<RetVal>::await_ready() noexcept
        {
            return false;
        }

        template<typename RetVal>
        inline void AwaitResumeOn<RetVal>::await_resume(CoroHandle h) noexcept
        {
            auto& promise = h.promise();
            //set thread_index£¿£¿
            Schedule(promise);
        }

        template<typename U>
        inline void YieldAwaiter<U>::await_suspend(impl::coroutine_handle<CoroPromise<U>> h) noexcept
        {
            AWAIT_SUSPEND_PROC();
            return;
        }

        inline void YieldAwaiter<void>::await_suspend(impl::coroutine_handle<CoroPromise<void> > h) noexcept
        {
            AWAIT_SUSPEND_PROC();
            return;
        }

        template<typename U>
        inline bool FinalAwaiter<U>::await_suspend(impl::coroutine_handle<CoroPromise<U>> h) noexcept
        {
            AWAIT_SUSPEND_PROC();
            return promise.IsParentCoro();
        }

        inline bool FinalAwaiter<void>::await_suspend(impl::coroutine_handle<CoroPromise<void>> h) noexcept
        {
            AWAIT_SUSPEND_PROC();
            return promise.IsParentCoro();
        }

        namespace CoroPromiseSelfDeconstructor
        {
            template<typename T>
            void Dealloc(JobEntry* job) {
                using PromiseType = CoroPromise<T>;
                auto* promise = static_cast<PromiseType*>(job);
                auto coro = impl::coroutine_handle<PromiseType>::from_promise(*promise);
                if (coro != nullptr) {
                    coro.destroy();
                }
            }

            template<>
            void Dealloc<void>(JobEntry* job) {
                using PromiseType = CoroPromise<void>;
                auto* promise = static_cast<PromiseType*>(job);
                auto coro = impl::coroutine_handle<PromiseType>::from_promise(*promise);
                if (coro != nullptr) {
                    coro.destroy();
                }
            }
        }
       
        template<typename RetVal>
        inline CoroPromise<RetVal>::CoroPromise():CoroPromiseBase(CoroHandle::from_promise(*this))
        {
        }

        template<typename RetVal>
        inline JobEntry::JobDeAllocator CoroPromise<RetVal>::GetDeAllocator() const
        {
            return CoroPromiseSelfDeconstructor::Dealloc<RetVal>;
        }

        template<typename RetVal>
        inline void CoroPromise<RetVal>::return_value(RetVal val)
        {
            if (IsParentCoro())
            {
                value_ = std::make_pair(true, val);
            }
            else
            {
                *func_value_ = std::make_pair(true, val);
            }

        }

        template<typename RetVal>
        inline void CoroPromise<RetVal>::resume()  
        {
            if (!IsParentCoro() && func_value_.get() != nullptr) {
                (*func_value_).first = false;
            }
            if (coro_ && !coro_.done()) {
                coro_.resume();
            }
            return;
        }

        template<typename RetVal>
        inline YieldAwaiter<RetVal> CoroPromise<RetVal>::yield_value(RetVal val)
        {
            if (!IsParentCoro()) {
                *func_value_ = std::make_pair(true, val);
            }
            else {
                value_ = std::make_pair(true, val);
            }
            return {};
        }

        template<typename RetVal>
        inline Coro<RetVal> CoroPromise<RetVal>::get_return_object_on_allocation_failure()
        {
            LOG(ERROR) << "job system alloc coro failed";
            return {};
        }

        template<typename RetVal>
        inline Coro<RetVal> CoroPromise<RetVal>::get_return_object() noexcept
        {
            if (!IsParentCoro()) {
                *func_value_ = std::make_pair(false, RetVal{});
            }
            else {
                value_ = std::make_pair(false, RetVal{});
            }
            return Coro<RetVal>(Coro<RetVal>::CoroHandle::from_promise(*this));
        }

        template<typename RetVal> template<typename U>
        inline AwaitTuple<RetVal, U> CoroPromise<RetVal>::await_transform(U&& func) noexcept
        {
            return { std::tuple<U&&>(std::forward<U>(func)) };
        }

        template<typename RetVal> template<typename... Ts>
        inline AwaitTuple<RetVal, Ts...> CoroPromise<RetVal>::await_transform(std::tuple<Ts...>&& tuple) noexcept
        {
            return { std::forward<std::tuple<Ts...> >(tuple) };
        }

        template<typename RetVal> template<typename ...Ts>
        inline AwaitTuple<RetVal, Ts...> CoroPromise<RetVal>::await_transform(std::tuple<Ts...>& tuple) noexcept
        {
            return { std::forward<std::tuple<Ts...> >(tuple) };
        }

        inline CoroPromise<void>::CoroPromise():CoroPromiseBase(CoroHandle::from_promise(*this))
        {
        }

        inline JobEntry::JobDeAllocator CoroPromise<void>::GetDeAllocator()const
        {
            return CoroPromiseSelfDeconstructor::Dealloc<void>;
        }

        inline void CoroPromise<void>::resume()
        {
            if (!IsParentCoro() && func_value_.get() != nullptr) {
                func_value_->first = false;
            }
            if (coro_ && !coro_.done()) {
                coro_.resume();
            }
            return;
        }

        inline Coro<void> CoroPromise<void>::get_return_object_on_allocation_failure()
        {
            LOG(ERROR) << "coro alloc failed";
            return {};
        }

        inline Coro<void> CoroPromise<void>::get_return_object() noexcept
        {
            return Coro<void>(CoroHandle::from_promise(*this));
        }

        template<typename U>
        inline AwaitTuple<void, U> CoroPromise<void>::await_transform(U&& func) noexcept
        {
            return { std::tuple<U&&>(std::forward<U>(func)) };
        }

        template<typename... T>
        inline AwaitTuple<void, T...> CoroPromise<void>::await_transform(std::tuple<T...>&& tuple) noexcept
        {
            return { std::forward<std::tuple<T...> >(tuple) };
        }

        template<typename ...T>
        inline AwaitTuple<void, T...> CoroPromise<void>::await_transform(std::tuple<T...>& tuple) noexcept
        {
            return { std::forward<std::tuple<T...> >(tuple) };
        }

        template<typename T>
        requires std::is_base_of_v<CoroBase, std::decay_t<T> >
        decltype(auto) Schedule(T&& coro, JobEntry* parent)
        {
            auto promise = coro.promise();
            if (nullptr != promise) {
                promise->parent_ = parent;
                if (parent != nullptr) {
                    parent->ref_count_.fetch_add(1, std::memory_order::relaxed);
                }
                //set coro affinity todo
                promise->EnableSelfDeConstruct(); //delete by jobsystem
            }
            SimpleJobSystem::Instance().Execute(promise);
            return promise;
        }

        template<typename T>
        requires std::is_base_of_v<CoroBase, std::decay_t<T> >
        void Continuation(T&& coro)noexcept
        {
            auto* curr_job = SimpleJobSystem::Instance().GetCurrentJob();
            if (nullptr != curr_job && !curr_job->IsCoro()) //todo just copy
            {
                curr_job->subsequent_ = coro.Handle().promise();
                coro.Handle().promise().EnableSelfDeConstruct();
            }
        }

        namespace AwaitTupleHelper
        {
            template<typename ...Ts>
            decltype(auto) Parallel(Ts&& ...args)
            {
                return std::tuple<Ts&&...>(std::forward<Ts>(args)...);
            }

            template<typename T>
            Coro<> SequentialImpl(T&& coro) {
                co_await std::forward<T>(coro);
            }

            template<typename T, typename U, typename ...Ts>
            Coro<> SequentialImpl(T&& coro, U&& next, Ts&&... rest)
            {
                co_await std::forward<T>(coro);
                co_await SequentialImpl(next, std::forward<Ts>(rest)...);
            }

            template<typename ...Ts>
            decltype(auto) Sequential(Ts&&... args)
            {
                return SequentialImpl(std::forward<Ts>(args)...);
            }
        }
    }

}