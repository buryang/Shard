#pragma once
#include "Utils/CommonUtils.h"
#include <thread>
#include <atomic>
#include <optional>

namespace MetaInit
{
	namespace Utils
	{

		template<typename T>
		class RingBuffer
		{
		public:
			RingBuffer(const uint32_t capacity);
			std::optional<T> PopFront();
			RingBuffer& PushBack(T& data);
			uint32_t Capacity()const;
		private:
			DISALLOW_COPY_AND_ASSIGN(RingBuffer);
			Vector<T>	data_repo_;
			uint32_t	head_{ 0 };
			uint32_t	tail_{ 0 };
			const uint32_t			capacity_;
			std::atomic<bool>		lock_{false};
		};


		template<typename Function>
		struct MINIT_API JobEntry
		{
			std::atomic<uint32_t>	ref_count_{ 0 };
			JobEntry*				parent_{ nullptr };
			Function				task_;
			//task can streal by other thread?
			bool					stealable_{ false };
			void Release();
			~JobEntry();
		};

		//class coroutine return object 
		template<typename Function, typename RetVal = void>
		class MINIT_API Coro
		{
		public:
			bool ready();
			RetVal get_value();
		private:
			CoroJobEntry<Function, RetVal>		promise_;
		};

		//class awaitable class tuple
		template<typename... Ts>
		class AwaitTuple : std::suspend_always
		{
		private:
			std::tuple<Ts&...>		tuple_;
		};

		template<typename Function, typename RetVal = void>
		class MINIT_API CoroJobEntry : public JobEntry
		{
		public:
			std::suspend_always initial_suspend();
			void unhandled_exception();
			void final_suspend()
			void return_void() noexcept {}
			void return_value(RetVal t);
			Coro<RetVal> get_return_object() noexcept;
			//overload operator
			template<class Allocator, typename... Args>
			void* operator new(std::size_t size, std::allocator_arg_t, Allocator& alloc, Args&... args) noexcept;
			void operator delete(void* ptr, std::size_t size) noexcept;
		private:
			friend class Coro<Function, RetVal>;
			std::tuple<bool, RetVal>	value_;
		};

		class MINIT_API SimpleJobSystem
		{
		public:
			SimpleJobSystem(const uint32_t try_count) : max_try_count_(try_count) {}
			void Init(const uint32_t group_count);
			void UnInit();
			void Dispatch();
			void Execute(JobEntry<>* job);
			~SimpleJobSystem() { UnInit(); }
		private:
			DISALLOW_COPY_AND_ASSIGN(SimpleJobSystem);
		private:
			SmallVector<RingBuffer<JobEntry<>*> >		local_queues_;
			SmallVector<RingBuffer<JobEntry<>*> >		global_queues_;
			Vector<std::thread>			thread_pool_;
			std::atomic<bool>			is_terminated_{ false };
			const uint32_t				max_try_count_{ 15 };
		};
	}
}

#include "Utils/SimpleJobSystem.inl"