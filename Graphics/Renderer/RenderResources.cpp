#include "Renderer/RtRenderResources.h"

namespace Shard
{
    namespace Renderer
    {
        RtField& RtField::StageFlags(EPipelineStageFlags stage)
        {
            stage_ = stage;
            return *this;
        }

        RtField& RtField::CrossQueue(bool value)
        {
            is_cross_queue_ = value;
            return *this;
        }

        RtField& RtField::ForceTransiant()
        {
            is_transiant_ = true;
            return *this;
        }

        const String& RtField::GetName() const
        {
            return name_;
        }

        const RtField::Name& RtField::GetHashName() const
        {
            return hash_name_;
        }

        const RtField::Name& RtField::GetParentName() const
        {
            return parent_name_;
        }

        EPixFormat RtField::GetPixFormat() const
        {
            return pix_fmt_;
        }

        bool RtField::IsConnectAble(const RtField& other) const
        {
            /*only ouput field connect to other input one*/
            if (!IsOutput() || other.IsOutput()) {
                return false;
            }
            if (type_ != other.type_ || (dims_ != other.dims_&& other.dim_prop_ != ETextureDimType::eInputRelative)) {
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
            hash_name_ = RtField::Name::FromString(name);
            /*default parent be self*/
            parent_name_ = hash_name_;
            return *this;
        }

        RtField& RtField::ParentName(const RtField::Name& name)
        {
            parent_name_ = name;
            return *this;
        }

    }
}

