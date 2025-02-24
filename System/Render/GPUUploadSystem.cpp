#include "Utils/SimpleJobSystem.h"
#include "GPUUploadSystem.h"

namespace Shard::Render
{
    void GPUUploadSystem::Update(Utils::ECSSystemUpdateContext& ctx)
    {
        //todo iterate all uploader
    }

    size_type GPUDataUploader::DataSlotLinearAllocator::Allocate(size_type data_size)
    {
        //1/2 step: find free range matching
        for (auto n = 0; n < 2; ++n) {
            if (auto iter = eastl::find_if(free_ranges_.begin(), free_ranges_.end(), [](auto node) { return node.size_ >= data_size; });
                iter != free_ranges_.end()) {
                auto offset = iter->offset_;
                if (iter->size_ > data_size) {
                    DeAllocate(offset + data_size, iter->size_ - data_size);
                }
                free_ranges_.erase(iter);
                return offset;
            }
            if (!n) {
                Consolidate();
            }
        }
        //3 step allocate in original space
        if (curr_cursor_ + data_size <= capacity_) {
            return std::exchange(curr_cursor_, curr_cursor_ + data_size);
        }
        return -1;
    }

    void GPUDataUploader::DataSlotLinearAllocator::DeAllocate(size_type data_offset, size_type data_size) {
        pending_free_ranges_.emplace_back({ data_offset, data_size });
    }

    void GPUDataUploader::DataSlotLinearAllocator::Consolidate() {
        if (pending_free_ranges_.empty()) {
            return;
        }
        //sort pending free ranges
        eastl::sort(pending_free_ranges_.begin(), pending_free_ranges_.end(), [](auto lhs, auto rhs) {return lhs.offset_ >= rhs.offset_; });
        List<FreeSlot> temp;
        for (auto pend_iter = pending_free_ranges_.begin(), iter = free_ranges_.begin(); pend_iter != pending_free_ranges_.end()||iter != free_ranges_.end(); ) {
            auto range = iter != free_ranges_.end() ? *iter : FreeSlot{~0u: 0}; //a invalid slot
            //sort
            if (pend_iter != pending_free_ranges_.end() && pend_iter->offset_ < range.offset) {
                range = *pend_iter++;
            }
            else
            {
                ++iter;
            }
            if (range.size_ > 0u) {
                //merge
                if (!temp.empty() && temp.back().offset_ + temp.back().size_ == range.offset_) {
                    temp.back().size_ += range.size_;
                }
                else
                {
                    temp.emplace_back(range);
                }
            }
        }
        eastl::swap(free_ranges_, temp);
        pending_free_ranges_.clear();
    }

    void GPUUploaderStageWriter::Write(void* cpu_data, size_type size, UploadOperation::EOperation ops)
    {
        void* stage_dst{ nullptr };
        size_type ops_offset, data_offset;

        if (owner_->TryAllocStageBuffer(size, stage_dst, ops_offset, data_offset)) {
            //copy data to stage buffer
            UploadOperation operation{ .ops_ = ops };
            switch (ops) {
            case UploadOperation::EOperation::eRaw:
                operation.data_offset_ = 0u;
                operation.data_size_ = size;
                break;
            case UploadOperation::EOperation::eInverseTransform:
                break;//todo
            default:
                LOG(ERROR) << "load operation not supported";
            }
            memcpy((uint8_t*)stage_dst + ops_offset, &operation, sizeof(operation));
            memcpy((uint8_t*)stage_dst + data_offset, cpu_data, size);
            return;
        }
        LOG(ERROR) << "stage buffer for upload is overflow";
    }

    GPUUploaderStageWriter& GPUSparseUploader::TLSGetUploadWriter() 
    {
        thread_local GPUUploaderStageWriter upload_writer{this};
        return upload_writer;
    }

    void GPUSparseUploader::Begin()
    {
    }

    void GPUSparseUploader::EndAndSubmit(HAL::HALCommandContext& immediate_contnext)
    {
        {

        }

        operation_offset_ = 0u;
        data_offset_ = ~0u;
    }

    GPUSparseUploader::~GPUSparseUploader()
    {
    }

    bool GPUSparseUploader::TryAllocStageBuffer(size_type size, void*& dst, size_type& ops_offset, size_type& data_offset)
    {
        {
            Utils::SpinLock lock(alloc_lock_);
            const auto new_ops_offset = operation_offset_ + sizeof(UploadOperation);
            const auto new_data_offset = data_offset_ - size;
            if (new_data_offset <= new_ops_offset) {
                return false;
            }
            /*|ops-ops-ops------data-data-data|*/
            ops_offset = operation_offset_;
            operation_offset_ = new_ops_offset;
            data_offset = new_data_offset;
            data_offset_ = new_data_offset;
        }
        return false;
    }
}
