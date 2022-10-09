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
		RtField& RtField::ForceTransiant()
		{
			force_transiant_ = true;
			return *this;
		}
		RtField& RtField::Merge(const RtField& other)
		{
			if (IsMergeAble(other)) {
				if (IsWholeResource() && !other.IsWholeResource())
					InitAllSubResources(sub_resources_[0]);
				for (size_t n = 0; n < GetSubFieldCount(); ++n) {
					auto& lhs = sub_resources_[n];
					const auto& rhs = other.sub_resources_[n];
					//todo merge work
					lhs.access_ = Utils::LogicOrFlags(lhs.access_, rhs.access_);
					lhs.usage_ = Utils::LogicOrFlags(lhs.usage_, rhs.usage_);
				}
			}
			return *this;
		}

		RtField& RtField::InitWholeResource(const RtField::SubTextureField& init_state)
		{
			sub_resources_.resize(1);
			sub_resources_[0] = init_state;
		}
		RtField::SubTextureField& RtField::operator[](uint32_t index)
		{
			return sub_resources_[index];
		}
		const RtField::SubTextureField& RtField::operator[](uint32_t index) const
		{
			return sub_resources_[index];
		}
		RtField::SubTextureField& RtField::operator[](const SubRangeIndex& index)
		{
			auto index_num = index.mip_ + (index.layer_ + index.plane_*array_slices_)*mip_slices_;
			return (*this)[index_num];
		}
		const RtField::SubTextureField& RtField::operator[](const SubRangeIndex& index) const
		{
			auto index_num = index.mip_ + (index.layer_ + index.plane_ * array_slices_) * mip_slices_;
			return (*this)[index_num];
		}
		RtField& RtField::InitAllSubResources(const RtField::SubTextureField& init_state)
		{
			auto num_sub_resources = GetSubFieldCount();
			sub_resources_.resize(num_sub_resources);
			for (auto& sub_range : sub_resources_) {
				sub_range = init_state;
			}
			return *this;
		}
		uint32_t RtField::GetSubFieldCount() const
		{
			return mip_slices_ * array_slices_ * plane_slices_;
		}
		const String& RtField::GetName() const
		{
			return name_;
		}
		const String& RtField::GetParentName() const
		{
			return parent_name_;
		}
		const RtField::SubTextureField& RtField::GetWholeResource()const
		{
			return (*this)[0];
		}
		bool RtField::IsMergeAble(const RtField& other) const
		{
			if (!IsConnectAble(other)) {
				return false;
			}
			if (sub_resources_.size() != other.sub_resources_.size()) {
				return false;
			}
			else
			{
				for (auto n = 0; n < sub_resources_.size(); ++n) {
					//to check subresource mergeable
					if (!IsSubFieldMergeAllowed(sub_resources_[n], other.sub_resources_[n])) {
						return false;
					}
				}
			}
			return true;
		}

		bool RtField::IsConnectAble(const RtField& other) const
		{
			if (type_ != other.type_) {
				return false;
			}

			if (width_ != other.width_ || height_ != other.height_ ||
				depth_ != other.depth_ || plane_slices_ != other.plane_slices_ ||
				mip_slices_ != other.mip_slices_ || array_slices_ != other.array_slices_) {
				return false;
			}
			return true;
		}

		bool RtField::IsTransiant() const
		{
			if(IsExternal()) {
				return false;
			}
			if (IsOutput() && !force_transiant_) {
				return false;
			}
			return true;
		}

		RtField& RtField::Name(const String& name)
		{
			name_ = name;
			/*default parent be self*/
			parent_name_ = name;
		}

		RtField& RtField::ParentName(const String& name)
		{
			parent_name_ = name;
		}

	}
}