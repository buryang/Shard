#pragma once

namespace MetaInit::Utils
{
	Allocation RingBuffer::Alloc(size_t size)
	{
		Allocation result;
		if (head_ > tail_) {
			if (head_ + size < capacity_) {
				result.offset = head_;
				result.size_ = size;
				head_ += size;
			}
			else
			{
				if (tail_ > size) {
					head_ = size;
					result.offset = 0;
					result.size_ = size;
				}
				else {

				}
			}
		}
		else
		{
			if (head_ + size < tail_) {
				result.offset = head_;
				result.size_ = size;
				head_ += size;
			}
			else {

			}
		}

		return result;
	}

	bool RingBuffer::Free(Allocation alloc)
	{
		delete_alloc_.emplace_back(alloc);
		while (1) {
			auto iter = std::find_if(delete_alloc_.begin(), delete_alloc_.end(), [&, this](Allocation del_alloc) {
									return del_alloc.offset_ = tail_; });
			if (iter == delete_alloc_.end())
				break;
			tail_ += iter->size_;
			delete_alloc_.erase(iter);
		}
		return true;
	}

	Allocation LinearBuffer::Alloc(size_t size)
	{
		if (size + head_ > capacity_) {

		}

		Allocation result{ head_, size };
		head_ += size;
		return result;
	}
}
