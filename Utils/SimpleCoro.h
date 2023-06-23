#pragma once
#include "Utils/SimpleJobSystem.h"


namespace Shard
{
	namespace Utils
	{
		template<typename RetVal = void> class Coro;
		template<typename RetVal, typename... Ts> class AwaitTuple;
		template<typename RetVal = void> class CoroPromise;

		//class coroutine return object 
		template<typename RetVal = void>
		class Coro
		{
		public:
			using promise_type = CoroPromise<RetVal>;
			//https://en.cppreference.com/w/cpp/coroutine/coroutine_traits
			using PromiseType = promise_type;//for compiler to def promise type
			using CoroHandle = std::coroutine_handle<PromiseType>;
			Coro(CoroHandle handle) noexcept;
			bool ready();
			RetVal get_value();
			CoroHandle handle();
		private:
			DISALLOW_COPY_AND_ASSIGN(Coro);
		private:
			CoroHandle		handle_;
		};

		//class awaitable class tuple
		template<typename RetVal, typename... Ts>
		class AwaitTuple : std::suspend_always
		{
		public:
			using CoroHandle = std::coroutine_handle <CoroPromise<std::function<void(void)>, RetVal>;
			AwaitTuple(std::tuple<Ts&&...> tuple);
			constexpr bool await_ready() noexcept override;
			constexpr bool await_suspend(CoroHandle handle)noexcept override;
			decltype(auto) await_resume()override;
		private:
			std::tuple<Ts&...>		tuple_;
		};

		template<typename RetVal = void>
		class  CoroPromise : public JobEntry
		{
		public:
			using TaskEntry = JobEntry::TaskEntry < std::coroutine_handle<CoroPromise<RetVal> >;
			CoroPromise();
			std::suspend_always initial_suspend() { return {}; }
			void unhandled_exception();
			std::suspend_always final_suspend() { return {}; }
			void return_void() noexcept {}
			template<typename ValueType>
			void return_value(ValueType val);
			Coro<RetVal> get_return_object() noexcept;
			//overload operator
			template<class Allocator, typename... Args>
			void* operator new(std::size_t size, std::allocator_arg_t, Allocator& alloc, Args&&... args) noexcept;
			void operator delete(void* ptr, std::size_t size) noexcept;
			//job entry operator fixme to check corohandle done
			//void operator()(void)override;
			//await transform
			template<typename U>
			AwaitTuple<RetVal, U> await_transform(U&& func)noexcept;
			template<typename... Ts>
			AwaitTuple<RetVal, Ts...> await_transform(std::tuple<Ts...>&& tuple) noexcept;
		private:
			friend class Coro<RetVal>;
			std::tuple<bool, RetVal>	value_;
			TaskEntry					task_entry_;
		};

		template<typename T>
		requires std::is
		uint32_t Schedule(Coro<T>&& coro, JobEntry* parent);

		template<typename T>
		void Continuation(Coro<T>&& coro)noexcept;

		template<typename... Ts>
		decltype(auto) Parallel(Ts&&... args);
	}
}

#include "Utils/SimpleCoro.inl"
