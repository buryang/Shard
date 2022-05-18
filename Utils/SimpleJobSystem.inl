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

		template<typename Function>
		inline void JobEntry<Function>::Release()
		{
			--ref_count_;
			if (!ref_count_)
			{
				//todo 
			}
		}

		template<typename Function, typename RetVal>
		inline bool Coro<Function, RetVal>::ready()
		{
			return promise_.;
		}

		template<typename Function, typename RetVal>
		inline RetVal Coro<Function, RetVal>::get_value()
		{
			return RetVal();
		}

		template<typename Function, typename RetVal>
		inline std::suspend_always CoroJobEntry<Function, RetVal>::initial_suspend()
		{
			return std::suspend_always();
		}

		template<typename Function, typename RetVal>
		inline void CoroJobEntry<Function, RetVal, Allocator>::return_value(RetVal t)
		{
			value_ = std::make_tuple(true, t);
		}



	}

}