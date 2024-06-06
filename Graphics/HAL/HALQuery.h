#pragma once
#include "Utils/CommonUtils.h"

#ifdef DEVELOP_DEBUG_TOOLS
namespace Shard::HAL
{
    class HALCommandContext;
    class HALQueryPool
    {
    public:
        enum class EType
        {
            eOcclusion,
            eBinaryOcclusion,
            eTimeStamp,
            ePipelineStats,
        };
        HALQueryPool(EType type, uint32_t count, uint32_t stride, uint32_t queue_index):type_(type), count_(count), stride_(stride), queue_index_(queue_index) {
            const auto results_size = (uint32_t)std::ceil(count * stride / sizeof(uint64_t));
            read_back_results_.resize(results_size);
        }
        EType GetPoolType()const {
            return type_;
        }
        EType GetSupportQueryType()const {
            if (type_ == EType::eOcclusion) {
                return EType(Utils::EnumToInteger(EType::eBinaryOcclusion) | Utils::EnumToInteger(EType::eOcclusion));
            }
            return type_;
        }
        bool IsQueryReady()const {
            return read_back_;
        }
        const void* GetQueryResult(uint32_t index)const {
            return reinterpret_cast<const uint8_t*>(read_back_results_.data()) + index * stride_;
        }
        virtual void Reset() { read_back_ = false; };
        virtual void WaitReadBack() { read_back_ = true; };
        virtual uint64_t GetTimeStampFrequency() const = 0;
        virtual ~HALQueryPool() = default;
    protected:
        uint32_t    count_{ 0u };
        uint32_t    stride_{ sizeof(uint64_t) };
        uint32_t    queue_index_{ ~0u };
        EType   type_{ EType::eTimeStamp };
        bool    read_back_{ false };
        Vector<uint64_t>    read_back_results_;
    };
}
#endif
