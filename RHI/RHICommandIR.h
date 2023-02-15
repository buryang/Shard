#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHISync.h"
#include "RHI/RHIResources.h"

namespace MetaInit::RHI {

	struct RHICommandPacketInterface
	{
		using Ptr = RHICommandPacketInterface*;
		enum class ECommandType :uint32_t {
			eNone,
			eSetStreamSource,
			eSetViewPoint,
			eSetScissorRect,
			eSetPipelineState,
			eSetShader,
			eBeginPass,
			eEndPass,
			eDraw,
			eDrawIndirect,
			eDrawIndexed,
			eDrawIndexedIndirect,
			eDispatch,
			eDispatchIndirect,
			eBarrier,
			eCopyBuffer,
			eCopyTexture,
			eCopyBufferTexture,
			eClearTexture,
			eFillBuffer,
			eUpdateBuffer,
			eBlitImage,
			ePushConstant,
		};
		enum {
			Type = ECommandType::eNone,
		};
		virtual ~RHICommandPacketInterface() {}
	};

#define IMPLEMENT_TYPE(type) enum { Type = (type), }

	struct RHISetStreamSourcePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetStreamSource);
		uint32_t	stream_index_;
		uint32_t	offset_;
	};

	struct RHISetViewPointPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetViewPoint);
		vec3 min_dims_;
		vec3 max_dims_;
	};

	struct RHISetPipelineStatePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetPipelineState);
	};

	struct RHISetShaderPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetShader);
	};

	struct RHIBeginRenderPassPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eBeginPass);
		//other data
	};

	struct RHIEndRenderPassPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eEndPass);
	};

	struct RHIDrawIndexedPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eDrawIndexed);
		uint32_t base_vertex_{ 0 };
		uint32_t first_instance_{ 0 };
		uint32_t num_vertex_{ 0 };
		uint32_t num_instance_{ 0 };
	};

	struct RHIDrawIndexedIndirectPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eDrawIndexedIndirect);
		uint32_t draw_count_{ 0 };
		uint32_t first_argument_{ 0 };
		uint32_t stride_{ 0 };
	};

	struct RHIDispatchPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eDispatch);
		vec3 thread_grp_size_;
	};

	struct RHIDispatchIndirectPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eDispatchIndirect);
		uint32_t offset_{ 0 };
	};

	struct RHIBarrierPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eBarrier);
		RHITransitionInfo::Ptr	trans_info_{ nullptr };
	};

	struct RHICopyBufferPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eCopyBuffer);
		RHIBuffer::Ptr	src_buffer_{ nullptr };
		size_t	src_offset_{ 0 };
		RHIBuffer::Ptr dst_buffer_{ nullptr };
		size_t	dst_offset_{ 0 };
		size_t	size_{ 0 };
	};

	struct RHICopyTexturePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eCopyTexture);
		RHITexture::Ptr src_texture_{ nullptr };
		RHITexture::Ptr	dst_texture_{ nullptr };
		uint32_t	src_mip_{ 0 };
		uint32_t	dst_mip_{ 0 };
		uint32_t	mip_count_{ 0 };
		uint32_t	xxx_{0};//todo FIXME
	};

	struct RHICopyBufferTexturePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eCopyBufferTexture);
		enum class EType : uint8_t {
			eBufferToTexture,
			eTextureToBuffer,
		};
		EType	type_:
		RHIBuffer::Ptr	buffer_{ nullptr };
		RHITexture::Ptr	texture_{ nullptr };
		TextureSubRange sub_range_;
	};

	struct RHIClearBufferPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eClearTexture);
		RHITexture::Ptr	texture_{ nullptr };
		union {
			struct {
				union {
					vec4	float32_{ 0 };
					ivec4	int32_;
					uvec4	uint32_;
				};
			}color_;
			struct {
				float depth_;
				uint32_t stentcil_;
			}depth_stencil_;
		};
		TextureSubRange	sub_range_;
	};

	struct RHIFillBufferPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eFillBuffer);
		RHIBuffer::Ptr buffer_{ nullptr };
		size_t	offset_{ 0 };
		size_t	size_{ 0 };
		uint32_t	data_;
	};

	struct RHIBlitImagePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eBlitImage);
		RHITexture::Ptr	src_texture_{ nullptr };
		RHITexture::Ptr	dst_texture_{ nullptr };
	};

	struct RHIPushConstant final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::ePushConstant);
		uint32_t	flags_{ 0 };
		uint32_t	offset_{ 0 };
		Span<uint8_t>	user_data_;
	};
}