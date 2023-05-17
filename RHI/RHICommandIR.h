#pragma once
#include "Utils/CommonUtils.h"
#include "Renderer/RtRenderShaderParameters.h"
#include "RHI/RHISync.h"
#include "RHI/RHIResources.h"
#include "RHI/RHIShader.h"

namespace MetaInit::RHI {

	struct RHICommandPacketInterface
	{
		using Ptr = RHICommandPacketInterface*;
		enum class ECommandType :uint32_t {
			eNone,
			eSetStreamSource,
			eSetResource,
			eSetViewPoint,
			eSetScissorRect,
			eSetPipelineState,
			eSetShader,
			eBindPSO,
			eBeginPass,
			eEndPass,
			eNextSubPass,
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
			//todo deal with descriptor set update
			eSetTexture,
			eSetSRV,
			eSetUAV,
			eSetBuffer,
			eSetUniformBuffer,
			eSetRawParameter,
			eBlitImage,
			ePushConstant,
			//for event
			eSetEvent,
			eWaitEvent,
			eResetEvent,
		};
		virtual ECommandType GetType() = 0;
		virtual ~RHICommandPacketInterface() {}
	};

#define IMPLEMENT_TYPE(type) \
	RHICommandPacketInterface::ECommandType GetType() override \
	{\
		return type;\
	}

	struct RHISetStreamSourcePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetStreamSource);
		uint32_t	stream_index_;
		uint32_t	offset_;
	};

	struct RHISetReourcePacket final : public RHICommandPacketInterface {
		using EResourceType = Renderer::EShaderResourceType;
		IMPLEMENT_TYPE(ECommandType::eSetResource);
		EResourceType	resource_type_;
		uint32_t	base_index_{ 0u };
		uint32_t	buffer_index_{ 0u };
		union {
			void* trival_data_{ nullptr };
			RHIBuffer::Ptr	buffer_;
			RHITexture::Ptr texture_;
			RHISampler::Ptr	sampler_;
		};
	};

	struct RHISetViewPointPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetViewPoint);
		vec3	min_dims_;
		vec3	max_dims_;
	};

	struct RHISetScissorPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetScissorRect);
		vec4	scissor_;
	};

	struct RHISetPipelineStatePacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetPipelineState);
	};

	struct RHISetShaderPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eSetShader);
	};

	struct RHIBindPSOPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eBindPSO);
		enum EBindPoint {
			eGFX,
			eAsync,
			eRayTrace
		};
		RHI::RHIPipelineStateObject::Ptr	pso_{ nullptr };
		EBindPoint	bind_point_{ EBindPoint::eGFX };
	};

	struct RHIBeginRenderPassPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eBeginPass);
		RHI::RHITexture::Ptr	render_target_;
		vec4	roi_; //offset.x offset.y wxh
		vec4	clear_val_;
		//other data
	};

	struct RHIEndRenderPassPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eEndPass);
	};

	struct RHINextSubPassPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eNextSubPass);
		uint32_t	reserved_flags_{ 0u };
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
		RHIShader::Ptr	shader_{ nullptr };
		vec3 thread_grp_size_;
	};

	struct RHIDispatchIndirectPacket final : public RHICommandPacketInterface {
		IMPLEMENT_TYPE(ECommandType::eDispatchIndirect);
		RHIShader::Ptr	shader_{ nullptr };
		RHIBuffer::Ptr	args_buffer_{ nullptr };
		uint32_t	args_offset_{ 0u };
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