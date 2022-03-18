#include "Renderer/RtRenderResources.h"

namespace MetaInit
{
	namespace Renderer
	{
		uint32_t RtField::GetSubFieldCount() const
		{
			return mip_slices_ * array_slices_ * plane_slices_;
		}
		bool RtField::IsMergeAble(const RtField& other) const
		{
			return false;
		}
		bool RtField::IsTransiantAble(const RtField& dst) const
		{
			return false;
		}
	}
}