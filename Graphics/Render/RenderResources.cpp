#include "Render/RenderResources.h"

namespace Shard
{
    namespace Render
    {
        Field& Field::StageFlags(EPipelineStageFlags stage)
        {
            stage_ = stage;
            return *this;
        }

        Field& Field::CrossQueue(bool value)
        {
            is_cross_queue_ = value;
            return *this;
        }

        Field& Field::ForceTransiant()
        {
            is_transiant_ = true;
            return *this;
        }

        const String& Field::GetName() const
        {
            return name_;
        }

        const Field::Name& Field::GetHashName() const
        {
            return hash_name_;
        }

        const Field::Name& Field::GetParentName() const
        {
            return parent_name_;
        }

        EPixFormat Field::GetPixFormat() const
        {
            return pix_fmt_;
        }

        bool Field::IsConnectAble(const Field& other) const
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

        bool Field::IsTransiant() const
        {
            if(IsExternal()) {
                return false;
            }
            if (IsOutput() && !force_transiant_) {
                return false;
            }
            return true;
        }

        Field& Field::NameAs(const String& name)
        {
            name_ = name;
            hash_name_ = Field::Name::FromString(name);
            /*default parent be self*/
            parent_name_ = hash_name_;
            return *this;
        }

        Field& Field::ParentNameAs(const Field::Name& name)
        {
            parent_name_ = name;
            return *this;
        }

        RenderBuffer::BufferRepo& RenderBuffer::BufferRepoInstance()
        {
            static BufferRepo buffer_repo{xx};
            return buffer_repo;
        }

        RenderTexture::TextureRepo& RenderTexture::TextureRepoInstance()
        {
            static TextureRepo texture_repo{};
            return texture_repo;
        }

        void RenderTexture::MergeField(const Field& curr_field)
        {
        }

        void RenderTexture::SetRHI()
        {
        }
    }
}



