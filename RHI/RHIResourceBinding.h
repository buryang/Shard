#pragma once
#include "Utils/CommonUtils.h"
#include "Core/RenderGlobalParams.h"
#include "RHI/RHIResources.h"
#include "RHI/RHICommand.h"
#include "RHI/RHIGlobalEntity.h"

//main idea copy from https://blog.traverseresearch.nl/bindless-rendering-setup-afeb678d77fc
namespace Shard::RHI {

	REGIST_PARAM_TYPE(BOOL, RHI_RESOURCE_BINDLESS, true);

	struct RHIResourceHandle {
		uint32_t	index_ : 23;
		uint32_t	tag_ : 3;
		uint32_t	writable_ : 1;
		uint32_t	version_ : 5;
		FORCE_INLINE operator const uint32_t()const {
			return *reinterpret_cast<const uint32_t*>(this);
		}
		FORCE_INLINE operator uint32_t() {
			return *reinterpret_cast<uint32_t*>(this);
		}
	};

	struct RHINotifyHeader {
		uint32_t	flags_{ 0 };
		uint32_t	offset_{ 0 };
		uint32_t	size_{ 0 };
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

	struct RHIBindLessPushRange {
		uint32_t	offset_{ 0u };
		uint32_t	size_{ 0u };
	};

	//type and max size for each descrptor table entity
	struct RHIBindLessTableInitializer {
		enum {
			MAX_BINDINGLESS_MEMBERS = EBindLessTag::eNum,
		};
		struct Member {
			uint32_t	tag_{ 0u };
			uint32_t	max_size_{ 0u };
			void* extra_info_{ nullptr };
		};
		Member	members_[MAX_BINDINGLESS_MEMBERS];
		uint32_t	num_member_{ 0u };
	};

	class RHIResourceBindlessHeap
	{
	public:
		using Ptr = RHIResourceBindlessHeap*;
		using SharedPtr = eastl::shared_ptr<RHIResourceBindlessHeap>;
		virtual void Init(const RHIBindLessTableInitializer& desc) {}
		virtual void Bind(RHICommandContext::Ptr command) = 0;
		virtual RHIResourceHandle WriteBuffer(RHIBuffer::Ptr buffer) = 0;
		virtual RHIResourceHandle WriteTexture(RHITexture::Ptr texture) = 0;
		//push constant
		virtual void Notify(const RHINotifyHeader& header, const Span<uint8_t>& notify_data) {}
		FORCE_INLINE void Free(RHIResourceHandle handle) {
			free_index_.emplace_back(handle);
		}
		FORCE_INLINE bool IsBind() const {
			return cmd_buffer_ != nullptr;
		}
		virtual ~RHIResourceBindlessHeap() {}
	protected:
		RHIResourceHandle GetAvailableResourceHandle(uint32_t tag) {
			RHIResourceHandle handle;
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
		List<RHIResourceHandle>	free_index_;
		uint32_t	curr_max_handle_{ 0 };
		RHICommandContext::Ptr cmd_buffer_{ nullptr };
	};

}