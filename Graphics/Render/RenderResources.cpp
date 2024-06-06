#include "HAL/HALGlobalEntity.h"
#include "Render/RenderResources.h"

namespace Shard
{
    namespace Render
    {
        extern Utils::ScalablePoolAllocator<uint8_t>* g_render_allocator;
        Field& Field::SampleCount(const uint32_t sample_count)
        {
            sample_count_ = sample_count;
            return *this;
        }
        Field& Field::StageFlags(EPipelineStageFlags stage)
        {
            stage_ = stage;
            return *this;
        }

        Field& Field::Type(EType type)
        {
            type_ = type;
            return *this;
        }

        Field& Field::State(TextureState state)
        {
            sub_state_ = state;
            return *this;
        }

        Field& Field::Usage(EUsage usage)
        {
            usage_ = usage;
            return *this;
        }

        Field& Field::AuxFlags(EAuxFlags flags)
        {
            aux_flags_ = flags;
            return *this;
        }

        Field::EType Field::GetType() const
        {
            return type_;
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

        const String& Field::GetDescription() const
        {
            return description_;
        }

        const TextureDim& Field::GetDimension() const
        {
            return dims_;
        }

        const uint32_t Field::GetSampleCount() const
        {
            return sample_count_;
        }

        const TextureState& Field::GetSubFieldState() const
        {
            return sub_state_;
        }

        const TextureSubRangeRegion& Field::GetTextureSubRange() const
        {
            assert(type_ == EType::eTexture && "buffer resource does not have texture region");
            return texture_sub_range_;
        }

        const BufferSubRangeRegion& Field::GetBufferSubRange() const
        {
            assert(type_ == EType::eBuffer &&"texture resource does not have buffer region");
            return buffer_sub_range_;
        }

        EPixFormat Field::GetPixFormat() const
        {
            return pix_fmt_;
        }

        EPipelineStageFlags Field::GetPipelineStage() const
        {
            return stage_;
        }

        Field::EAuxFlags Field::GetAuxFlags() const
        {
            return aux_flags_;
        }

        Field Field::CreateDSVField(const String& name)
        {
            Field dsv_field{};
            dsv_field.NameAs(name).Type(EType::eTexture);
            dsv_field.State({ EAccessFlags::eDSV, ELayoutFlags::eOptimal });
            return dsv_field;
        }

        Field Field::CreateSRVField(const String& name)
        {
            Field srv_field{};
            srv_field.NameAs(name).Type(EType::eTexture);
            srv_field.State({ EAccessFlags::eSRV, ELayoutFlags::eOptimal });
            return srv_field;
        }

        Field Field::CreateRTVField(const String& name)
        {
            Field rtv_field{};
            rtv_field.NameAs(name).Type(EType::eTexture);
            rtv_field.State({ EAccessFlags::eRTV, ELayoutFlags::eOptimal });
            return rtv_field;
        }

        Field Field::CreateUAVField(const String& name)
        {
            Field uav_field{};
            uav_field.NameAs(name).Type(EType::eTexture);
            uav_field.State({ EAccessFlags::eUAV, ELayoutFlags::eOptimal });
            return uav_field;
        }

        bool Field::IsConnectAble(const Field& other) const
        {
            /*only ouput field connect to other input one*/
            if (!IsOutput() || other.IsOutput()) {
                return false;
            }
            if (usage_ == EUsage::eCulled) {
                return false;
            }
            if (type_ != other.type_ || (dims_ != other.dims_&& other.dim_prop_ != ETextureDimType::eInputRelative)) {
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

        Field& Field::Description(const String& description)
        {
            description_ = description;
            return *this;
        }

        Field& Field::ParentName(const Field::Name& name)
        {
            parent_name_ = name;
            return *this;
        }

        Field& Field::Width(const uint32_t width)
        {
            dims_.width_ = width;
            return *this;
        }

        Field& Field::Height(const uint32_t height)
        {
            dims_.height_ = height;
            return *this;
        }

        Field& Field::Depth(const uint32_t depth)
        {
            dims_.depth_ = depth;
            return *this;
        }

        Field& Field::MipSlices(const uint32_t mip_slices)
        {
            dims_.mip_slices_ = mip_slices;
            return *this;
        }

        Field& Field::ArraySlices(const uint32_t array_slices)
        {
            dims_.array_slices_ = array_slices;
            return *this;
        }

        Field& Field::PlaneSlices(const uint32_t plane_slices)
        {
            dims_.plane_slices_ = plane_slices;
            return *this;
        }
        
        RenderResource::RenderResource(const Field& field)
        {
        }

        bool RenderResource::CalcLifeTime(uint32_t& start, uint32_t& end)
        {
            uint32_t min_time{ std::numeric_limits<uint32_t>::max() }, max_time{ 0u };
            if (producer_passes_.empty() && consumer_passes_.empty()) {
                return false;
            }
            for (auto handle : producer_passes_) {
                min_time = Utils::Min(handle->GetDependencyLevel(), min_time);
                max_time = Utils::Max(handle->GetDependencyLevel(), max_time);
            }
            for (auto handle : consumer_passes_) {
                min_time = Utils::Min(handle->GetDependencyLevel(), min_time);
                max_time = Utils::Max(handle->GetDependencyLevel(), max_time);
            }
            start = min_time;
            end = max_time;
            return true;
        }

        RenderBuffer::BufferRepo& RenderBuffer::BufferRepoInstance()
        {
            static BufferRepo buffer_repo{ g_render_allocator };
            return buffer_repo;
        }

        RenderBuffer::RenderBuffer(const Field& field):RenderResource(field)
        {
        }

        void RenderBuffer::SetHAL()
        {
            HAL::HALBufferInitializer buffer_desc{};
            //todo transform renderbuffer desc to rhi desc
            rhi_buffer_ = HAL::HALGlobalEntity::Instance()->CreateStructedBuffer(buffer_desc); //todo
        }

        void RenderBuffer::EndHAL()
        {
            HAL::HALGlobalEntity::Instance()->ReleaseBuffer(rhi_buffer_);
            rhi_buffer_ = nullptr;
        }

        RenderTexture::TextureRepo& RenderTexture::TextureRepoInstance()
        {
            static TextureRepo texture_repo{ g_render_allocator };
            return texture_repo;
        }

        RenderTexture::RenderTexture(const Field& field):RenderResource(field)
        {
            curr_state_.push_back(field.GetSubFieldState());
        }

        RenderTexture::~RenderTexture()
        {
            if (rhi_texture_ != nullptr) {
                EndHAL();
            }
        }

        void RenderTexture::MergeField(const Field& curr_field)
        {
            const auto state_count = TextureSubRangeRegion::FromDims(curr_field.GetDimension()).GetSubRangeIndexCount(); //todo
            if (curr_field.IsWholeResource()) {
                if (IsWholeTexture()) {
                    //todo merge whole state
                    const auto& incr_state = curr_field.GetSubFieldState();
                    curr_state_.front();//todo
                }
                else
                {
                    assert(0);
                    LOG(ERROR) << "donnot realize subdivision texture to whole one conversion";
                }
            }
            else
            {
                if (IsWholeTexture()) {
                    curr_state_.resize(state_count);
                    eastl::fill(curr_state_.begin(), curr_state_.end(), curr_state_.front());
                }
                assert(curr_state_.size() == state_count);
                curr_field.GetTextureSubRange().Enumerate([&](auto) {}); //merge every substate    
            }
        }

        SmallVector<TextureState>& RenderTexture::GetState()
        {
            return curr_state_;
        }

        void RenderTexture::SetHAL()
        {
            HAL::HALTextureInitializer texture_desc{};

            rhi_texture_ = HAL::HALGlobalEntity::Instance()->CreateTexture(texture_desc);
        }
        void RenderTexture::EndHAL()
        {
            HAL::HALGlobalEntity::Instance()->ReleaseTexture(rhi_texture_);
            rhi_texture_ = nullptr;
        }
        bool RenderTexture::IsWholeTexture() const
        {
            return curr_state_.size() == 1u;
        }

        RenderTexture::Handle RenderResourceCache::GetOrCreateTexture(const Field& field)
        {
            const auto field_name = field.GetHashName();
            if (auto handle = TryGetTexture(field_name); handle.has_value()) {
                return handle.value();
            }
            const auto new_handle = RenderTexture::TextureRepoInstance().Alloc(field);
            NewTextureCacheNode(field_name, new_handle);
            return new_handle;
        }
        RenderBuffer::Handle RenderResourceCache::GetOrCreateBuffer(const Field& field)
        {
            const auto field_name = field.GetHashName();
            if (auto handle = TryGetBuffer(field_name); handle.has_value()) {
                return handle.value();
            }
            const auto new_handle = RenderBuffer::BufferRepoInstance().Alloc(field);
            NewBufferCacheNode(field_name, new_handle);
            return new_handle;
        }
        Optional<RenderTexture::Handle> RenderResourceCache::TryGetTexture(const Field::Name& field)
        {
            if (auto iter = texture_map_.find(field); iter != texture_map_.end()) {
                return texture_repos_[iter->second.index_];
            }
            return eastl::nullopt;
        }
        Optional<RenderBuffer::Handle> RenderResourceCache::TryGetBuffer(const Field::Name& field)
        {
            if (auto iter = buffer_map_.find(field); iter != buffer_map_.end()) {
                return buffer_repos_[iter->second.index_];
            }
            return eastl::nullopt;
        }
        bool RenderResourceCache::QueueExtractedTexture(const Field::Name& field, RenderTexture::Handle& texture)
        {
            if (auto iter = texture_map_.find(field); iter != texture_map_.end()) {
                iter->second.is_extracted_ = 1u;
                assert(iter->second.is_external_ != 1);
                texture = texture_repos_[iter->second.index_];
                return true;
            }
            return false;
        }
        bool RenderResourceCache::QueueExtractedBuffer(const Field::Name& field, RenderBuffer::Handle& buffer)
        {
            if (auto iter = buffer_map_.find(field); iter != buffer_map_.end()) {
                iter->second.is_extracted_ = 1u;
                assert(iter->second.is_external_ != 1);
                buffer = buffer_repos_[iter->second.index_];
                return true;
            }
            return false;
        }
        bool RenderResourceCache::RegistExternalBuffer(const Field::Name& external_field, RenderBuffer::Handle buffer)
        {
            if (auto iter = buffer_map_.find(external_field); iter != buffer_map_.end()) {
                LOG(ERROR) << "regist a external buffer field which exsited";
                return false;
            }
            auto& cache_node = NewBufferCacheNode(external_field, buffer);
            cache_node.is_external_ = 1u;
            return true;
        }
        bool RenderResourceCache::RegistExternalTexture(const Field::Name& external_field, RenderTexture::Handle texture)
        {
            if (auto iter = texture_map_.find(external_field); iter != texture_map_.end()) {
                LOG(ERROR) << "regist a external texture field which exsited";
                return false;
            }
            auto& cache_node = NewTextureCacheNode(external_field, texture);
            cache_node.is_external_ = 1u;
            return true;
        }
        void RenderResourceCache::Clear(bool non_extract)
        {
            if (non_extract) {
                for (const auto& [k, v] : texture_map_) {
                    if (!v.is_extract_ && !v.is_external_) {
                        texture_map_.erase(k);
                        auto iter = texture_repos_.begin() + v.index_;
                        RenderTexture::TextureRepoInstance().Free(*iter);
                        texture_repos_.erase(iter);
                    }
                }
                for (const auto& [k, v] : buffer_map_) {
                    if (!v.is_extract_ && !v.is_external_) {
                        buffer_map_.erase(k);
                        auto iter = buffer_repos_.begin() + v.index_;
                        RenderBuffer::BufferRepoInstance().Free(*iter);
                        buffer_repos_.erase(iter);
                    }
                }
            }
            else
            {
                texture_map_.clear();
                buffer_map_.clear();
                for (auto texture : texture_repos_) {
                    RenderTexture::TextureRepoInstance().Free(texture);
                }
                for (auto buffer : buffer_repos_) {
                    RenderBuffer::BufferRepoInstance().Free(buffer);
                }
                texture_repos_.clear();
                buffer_repos_.clear();
            }
        }
        RenderResourceCache::CacheNode& RenderResourceCache::NewTextureCacheNode(const Field::Name& field, RenderTexture::Handle texture)
        {
            texture_map_.insert(std::make_pair(field, RenderResourceCache::CacheNode{ (uint32_t)texture_repos_.size(), 0u, 0u }));
            texture_repos_.push_back(texture);
            return texture_map_[field];
        }
        RenderResourceCache::CacheNode& RenderResourceCache::NewBufferCacheNode(const Field::Name& field, RenderBuffer::Handle buffer)
        {
            buffer_map_.insert(std::make_pair(field, RenderResourceCache::CacheNode{ (uint32_t)buffer_repos_.size(), 0u, 0u }));
            buffer_repos_.push_back(buffer);
            return buffer_map_[field];
        }
}
}



