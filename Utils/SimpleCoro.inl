#include "SimpleCoro.h"
//https://github.com/hlavacs/ViennaGameJobSystem/blob/master/include/VGJSCoro.h

namespace MetaInit
{
	namespace Utils
	{

		class LockGuard
		{
		public:
			LockGuard(std::atomic<bool>& atomic) :atomic_(atomic)
			{
				bool expected = false;
				while (!atomic_.compare_exchange_weak(expected, true))
				{
					expected = false;
				}
			}
			~LockGuard()
			{
				atomic_.store(false);
			}
		private:
			std::atomic<bool>& atomic_;
		};

		template<typename T>
		RingBuffer<T>::RingBuffer(const uint32_t capacity) :capacity_(capacity)
		{
			data_repo_.resize(capacity);
		}

		template<typename T>
		std::optional<T> RingBuffer<T>::PopFront()
		{
			do
			{
				LockGuard(lock_);
				if (head_ != tail_)
				{
					head_ = (head_ + 1) % capacity_;
					return data_repo_.front();
				}
			} while (0);
			return std::nullopt;
		}

		template<typename T>
		RingBuffer<T>& RingBuffer<T>::PushBack(T& data)
		{
			do
			{
				LockGuard(lock_);
				auto cursor = (tail_ + 1) % capacity_;
				if (cursor != head_)
				{
					std::swap(data_repo_[cursor], data);
					tail_ = cursor;
				}
			} while (0);
			return *this;
		}

		template<typename T>
		inline uint32_t RingBuffer<T>::Capacity() const
		{
			return capacity_;
		}

		template<typename RetVal>
		inline bool Coro<RetVal>::ready()
		{
			return handle_.promise().value_.first;
		}

		template<typename RetVal>
		inline RetVal Coro<RetVal>::get_value()
		{
			return handle_.promise().value_.second;
		}

		template<typename RetVal>
		inline Coro<RetVal>::CoroHandle Coro<RetVal>::handle()
		{
			return handle_;
		}

		template<typename... Ts>
		inline constexpr bool AwaitTuple<...Ts>::await_ready() noexcept
		{
			uint32_t num = 0;
			template<typename U>
			std::size_t size(U & children)
			{
				if constexpr (std::is_same_v<std::decay_t<U>, Vector>) {
					return children.size();
				}

				return 1;
			}

			const auto f = [&]<std::size_t..index>(std::index_sequence<index...>) {
				num = (size(std::get<index>(tuple_)) + ... + 0);
			};
			f(std::make_index_sequence<sizeof...<Ts>>{});

			return !num;
		}

		template<typename RetVal, typename ...Ts>
		inline AwaitTuple<RetVal, Ts...>::AwaitTuple(std::tuple<Ts&&...> tuple):tuple_(tuple)
		{

		}

		template<typename ...Ts>
		inline constexpr bool AwaitTuple<...Ts>::await_suspend(AwaitTuple::CoroHandle handle) noexcept
		{
			auto promise = handle.promise();

			//single tuple element call
			const auto g = [&, this]<std::size_t index>() {
				using T = decltype(std::get<index>(tuple_));
				auto children = std::forward<T>(std::get<index>(tuple_));

				//todo schedule
				Schedule();
			};

			const auto f = [&, this]<std::size_t... index>(std::index_sequence<index...>) {
				(g.template operator() <index> (), ...);
			};

			f(std::make_index_sequence<sizeof...(Ts)>{});
			return true;//todo

		}

		template<typename ...Ts>
		inline decltype(auto) AwaitTuple<...Ts>::await_resume()
		{
			auto f = p&, this]<typename ...Us>(Us&... args) {
				return std::tuple_cat(get_val(args)...);
			};
			auto ret = std::apply(f, tuple_);
			if constexpr (std::tuple_size_v<decltype<ret>> == 0)
			{
				return;
			}
			else if constexpr (std::tuple_size_v<decltype<ret>> == 1)
			{
				return std::get<0>(ret);
			}

			return ret;
		}

		template<typename RetVal>
		inline CoroPromise<RetVal>::CoroPromise()
		{
			auto handle = std::coroutine_handle<>::from_promise(*this);
			task_entry_ = CoroPromise::TaskEntry(handle);
			//convert corohandle to task
			SetTask(&task_entry_);
		}

		template<typename RetVal>
		inline void CoroPromise<RetVal>::unhandled_exception()
		{
			//todo fixme with log system
			std::terminate();
		}

		template<typename RetVal>
		template<typename ValueType>
		inline void CoroPromise<RetVal>::return_value(ValueType val)
		{
			value_ = std::make_tuple(true, val);
		}

		template<typename RetVal>
		template<class Allocator, typename ...Args> 
		inline void* CoroPromise<RetVal>::operator new(std::size_t size, std::allocator_arg_t, Allocator& alloc, Args&& ...args) noexcept
		{
			auto raw_ptr = alloc.allocate(size);
			return raw_ptr;
		}

		template<typename RetVal>
		inline void CoroPromise<RetVal>::operator delete(void* ptr, std::size_t size) noexcept
		{
			//do nothing
		}

		template<typename RetVal, typename U>
		inline AwaitTuple<RetVal, U> CoroPromise<RetVal>::await_transform(U&& func) noexcept
		{
			return {std::tuple<U&&>(std::forward<U&&>(func)};
		}

		template<typename RetVal, typename... Ts>
		inline AwaitTuple<RetVal, Ts...> CoroPromise<RetVal>::await_transform(std::tuple<Ts...>&& tuple) noexcept
		{
			return { std::forward<std::tuple<Ts...>>(tuple); };
		}

		template<typename RetVal>
		inline Coro<RetVal> CoroPromise<RetVal>::get_return_object() noexcept
		{
			auto handle = Coro<Function, RetVal>::CoroHandle::from_promise(*this);
			return { handle };
		}

		template<typename T>
		uint32_t Schedule(Coro<T>&& coro, JobEntry* parent)
		{
			auto promise = coro.Handle().promise();
			if (nullptr != promise) {
				promise.parent_ = parent;
				parent.ref_count_.fetch_add(1, std::memory_order_acquire);
			}
			SimpleJobSystem::Instance().Execute(&promise);
		}

		template<typename ...Ts>
		decltype(auto) Parallel(Ts && ...args)
		{
			return std::tuple<Ts&&...>(std::forward<Ts>(args)...);
		}

	}

}