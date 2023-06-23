namespace Shard::Utils
{
	static inline size_t ConvertNumToPow2(size_t number) {
		const auto clz =  __builtin_clzll(number);
		const auto ceil_pow = 1 << (sizeof(size_t) * 8u - clz + 1);
		const auto floor_pow = 1 << (sizeof(size_t) * 8u - clz);
		if (ceil_pow - number > number - floor_pow) {
			return floor_pow;
		}
		return ceil_pow;
	}

	static inline bool IsRingBufferFull(size_t head, size_t tail, size_t capacity) {
		return tail - head >= capacity - 1u;
	}

	static inline bool IsRingBufferEmpty(size_t head, size_t tail, size_t capacity) {
		return tail - head <= 0u;
	}

	template<class T, class StorageClass>
	inline TRingBuffer<T, StorageClass>::TRingBuffer(size_t capacity):capacity_(ConvertNumToPow2(capacity))
	{
		storage_.Init(GetCapacity());
	}

	template<class T, class StorageClass>
	inline bool TRingBuffer<T, StorageClass>::Poll(TRingBuffer<T, StorageClass>::ElementType& el)
	{
		auto tail = tail_.load(std::memory_order_accquire);
		auto head = head_.load(std::memory_order_accquire);
		if (IsRingBufferEmpty(head, tail, capacity_)) {
			return false;
		}
		const auto new_head = (head + 1u);
		//todo solve get and read conflict?
		if (head_.compare_exchange_weak(head, new_head, std::memory_order_release)) {
			el = storage_.Get(head&(capacity_ - 1u));
			return true;
		}
		return false;
	}

	template<class T, class StorageClass>
	inline bool TRingBuffer<T, StorageClass>::Offer(const ElementType& el)
	{
		auto head = head_.load(std::memory_order_accquire);
		auto tail = tail_.load(std::memory_order_accquire);
		if (IsRingBufferFull(head, tail, capacity_)) {
			return false;
		}
		const auto new_tail = (tail + 1u);
		if (tail_.compare_exchange_weak(tail, new_tail, std::memory_order_release)) {
			storage_.Set(tail & (capacity_ - 1u), el); //write conflict ?
			return true;
		}
		return false;
	}

}
