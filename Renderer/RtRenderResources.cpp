#include "Renderer/RtRenderResources.h"

namespace MetaInit
{
	namespace Renderer
	{
		RtField& RtField::StageFlags(PipelineStageFlags stage)
		{
			stage_ = stage;
			return *this;
		}
		RtField& RtField::IncrRef(uint32_t reference)
		{
			reference_ += reference;
			return *this;
		}

		RtField& RtField::DecrRef(uint32_t reference)
		{
			reference_ -= reference;
			assert(reference_ >= 0);
			return *this;
		}
		RtField& RtField::ForceTransiant()
		{
			force_transiant_ = true;
			return *this;
		}
		uint32_t RtField::GetFlags() const
		{
			return uint32_t();
		}
		uint32_t RtField::GetSubFieldCount() const
		{
			return mip_slices_ * array_slices_ * plane_slices_;
		}
		bool RtField::IsMergeAble(const RtField& other) const
		{
			assert(type_ == other.type_);

			if (state_ != other.state_) {
				return false;
			}

			if () {
				return false;
			}
			return true;
		}
		bool RtField::IsTransitionNeeded(const RtField& dst) const
		{
			if (state_ != dst.state_ || ) {
				return true;
			}
			return false;
		}
		bool RtField::IsTransiant() const
		{
			if (IsExternal() || IsOutput() || force_transiant_) {
				return true;
			}
			return false;
		}
		void RtRenderResourceManager::AllocateResource(RtField* field)
		{

		}
		void RtRenderResourceManager::AllocateAllResource()
		{
			for (auto& [src_field, dst_field] : field_used_) {
				AllocateResource(dst_field);
			}
		}
		void RtRenderResourceManager::ReleaseResource()
		{
		}

		void RtRenderResourceManager::CreateOrGetBuffer(RtField& buffer_field)
		{
			auto it = field_to_resource_.find(buffer_field);
			if (field_to_resource_.end() != it) {
				return it->
			}
			else
			{

			}
		}
	}
}