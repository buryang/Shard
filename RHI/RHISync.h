#pragma once
#include "Utils/CommonUtils.h"
#include "Renderer/RtRenderBarrier.h"
#include "RHI/RHICommand.h"

namespace MetaInit::RHI {
	/*max buffer size for transition info*/
	static constexpr uint32_t MAX_TRANSITION_INFO_SIZE = 128;
	extern constexpr uint32_t __declspec(selectany) ALIGN_TRANSITION_INFO_SIZE = -1; //FIXME
	struct RHITransitionInfo {
		using Ptr = RHITransitionInfo*;
		template<typename MetaType>
		FORCE_INLINE auto* GetMetaData() {
			return reinterpret_cast<MetaType*>(meta_buffer + ALIGN_TRANSITION_INFO_SIZE);
		}
		template<typename MetaType>
		FORCE_INLINE const auto* GetMetaData() const {
			return const_cast<RHITransitionInfo*>(this)->GetMetaData();
		}
		template<typename MetaType>
		void Release() {
			GetMetaData<MetaType>()->~MetaType();
		}
		uint8_t	meta_buffer_[MAX_TRANSITION_INFO_SIZE];
	};

	class RHITransitionQueue
	{
	public:
		void Enqueue(Span<RHITransitionInfo::Ptr>& trans);
		void Submit(RHICommandContext::Ptr cmd);
	private:
		SmallVector<>;
	};

	void RHICreateTransition(const Renderer::RtBarrierBatch& barrier_info, RHITransitionInfo::Ptr& trans);
	void RHIBeginTransition(RHICommandContext::Ptr cmd, const RHITransitionInfo::Ptr trans);
	void RHIEndTransition(RHICommandContext::Ptr cmd, const RHITransitionInfo::Ptr trans);
}