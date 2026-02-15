#pragma once
#include "Utils/CommonUtils.h"
#include "Core/EngineGlobalParams.h"
#include "HAL/HALResources.h"
#include "HAL/HALCommand.h"
#include "HAL/HALGlobalEntity.h"

//main idea copy from https://blog.traverseresearch.nl/bindless-rendering-setup-afeb678d77fc
namespace Shard::HAL {

    REGIST_PARAM_TYPE(BOOL, HAL_RESOURCE_BINDLESS, true);

    struct HALResourceHandle {
        uint32_t    index_ : 23;
        uint32_t    tag_ : 3;
        uint32_t    writable_ : 1;
        uint32_t    version_ : 5;
        FORCE_INLINE operator const uint32_t()const {
            return *reinterpret_cast<const uint32_t*>(this);
        }
        FORCE_INLINE operator uint32_t() {
            return *reinterpret_cast<uint32_t*>(this);
        }
    };

    struct HALNotifyHeader {
        uint32_t    flags_{ 0 };
        uint32_t    offset_{ 0 };
        uint32_t    size_{ 0 };
    };

    enum EBindLessTag
    {
        eUnkownTag = 0x0,
        eBufferSRVTag = 0x1,
        eBufferUAVTag = 0x2,
        eTextureSRVTag = 0x3,
        eTextureSRVSamplerTag = 0x4,
        eTextureUAVTag = 0x5,
        eAccStructTag = 0x6,
        eNum,
    };

    enum EBindLessRangeTag
    {
        ePushRangeTag = EBindLessTag::eNum+1,
    };

    struct HALBindLessPushRange {
        uint32_t    offset_{ 0u };
        uint32_t    size_{ 0u };
    };

    //type and max size for each descrptor table entity
    struct HALBindLessTableInitializer {
        enum {
            MAX_BINDINGLESS_MEMBERS = EBindLessTag::eNum,
        };
        struct Member {
            uint32_t    tag_{ 0u };
            uint32_t    max_size_{ 0u };
            uint32_t    space_{ 0u };
            uint32_t    index_{ 0u };
            void* extra_info_{ nullptr };
        };
        Member  members_[MAX_BINDINGLESS_MEMBERS];
        uint32_t    num_member_{ 0u };
        static const HALBindLessTableInitializer& GetBindlessTableInitializer();
    };

    class HALResourceBindlessHeap
    {
    public:
        using Ptr = HALResourceBindlessHeap*;
        using SharedPtr = eastl::shared_ptr<HALResourceBindlessHeap>;
        virtual void Init(const HALBindLessTableInitializer& desc) {}
        virtual void UnInit() {}
        virtual void Bind(HALCommandContext* command) = 0;
        virtual HALResourceHandle WriteBuffer(HALBuffer* buffer) = 0;
        virtual HALResourceHandle WriteTexture(HALTexture* texture) = 0;
        //push constant
        virtual void Notify(const HALNotifyHeader& header, const Span<uint8_t>& notify_data) {}
        FORCE_INLINE void Free(HALResourceHandle handle) {
            std::unique_lock<std::shared_mutex> lock(free_mutex_);
            free_index_.emplace_back(handle);
        }
        FORCE_INLINE bool IsBind() const {
            return cmd_buffer_ != nullptr;
        }
        virtual ~HALResourceBindlessHeap() = default;
    protected:
        HALResourceHandle GetAvailableResourceHandle(uint32_t tag) {
            HALResourceHandle handle;
            std::unique_lock<std::shared_mutex>	lock(free_mutex_);
            if (!free_index_.empty()) {
                handle = free_index_.front();
                handle.tag_ = tag;
                free_index_.pop_front();
            }
            else {
                handle = { .index_ = curr_max_handle_++, .tag_ = tag, .version_ = 0 };
            }
            return handle;
        }
    protected:
        std::shared_mutex    free_mutex_;
        List<HALResourceHandle>    free_index_;
        uint32_t    curr_max_handle_{ 0u };
        HALCommandContext*  cmd_buffer_{ nullptr };
    };

}