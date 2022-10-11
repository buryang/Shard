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
		const String& RtField::GetName() const
		{
			return name_;
		}
		const String& RtField::GetParentName() const
		{
			return parent_name_;
		}
		bool RtField::IsConnectAble(const RtField& other) const
		{
			if (type_ != other.type_ || layout_ != other.layout_) {
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