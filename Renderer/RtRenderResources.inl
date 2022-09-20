#pragma once

namespace MetaInit
{
	namespace Renderer
	{
		template<class RHIHandle>
		inline void RtRenderResource<RHIHandle>::Init(const RtField& field)
		{
			is_imported_ = field.IsExternal();
			is_transiant_ = field.IsTransiant();
		}
	}
}