#include "RtRenderResources.h"
#pragma once

namespace Shard
{
	namespace Renderer
	{
		template<class RHIHandle>
		inline void RtRenderResource<RHIHandle>::Init(const RtField& field)
		{
			is_imported_ = field.IsExternal();
			is_transiant_ = field.IsTransiant();
			is_output_ = field.IsOutput();
		}
		template<class RHIHandle>
		inline bool RtRenderResource<RHIHandle>::IsExternal() const
		{
			return is_imported_;
		}
		template<class RHIHandle>
		inline bool RtRenderResource<RHIHandle>::IsTransient() const
		{
			return is_transient_;
		}
		template<class RHIHandle>
		inline bool RtRenderResource<RHIHandle>::IsOutput() const
		{
			return is_output_;
		}
	}
}