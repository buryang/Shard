#pragma once
#include "Utils/CommonUtils.h"
#include "RHI/RHISync.h"

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
		};
		virtual ECommandType Type() const {
			return ECommandType::eNone;
		}
		virtual ~RHICommandPacketInterface() {}
	};

#define IMPLEMENT_TYPE(type) ECommandType Type() const override{ return type; }

	struct RHISetStreamSroucePacket final : public RHICommandPacketInterface {
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
}