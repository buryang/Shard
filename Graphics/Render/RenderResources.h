#pragma once
#include "Utils/CommonUtils.h"
#include "Utils/Handle.h"
#include "Core/PixelInfo.h"
#include "RHI/RHIResources.h"
#include "RHI/RHIGlobalEntity.h"
#include "Render/RenderDefinitions.h"
#include <type_traits>

namespace Shard
{
    namespace Render
    {
        /*buffer use TextureDim width to represent size*/
        struct TextureDim {
            uint32_t    width_{ 0 };
            uint32_t    height_{ 0 };
            uint32_t    depth_{ 0 };
            uint32_t    mip_slices_{ 1 };
            uint32_t    array_slices_{ 1 };
            uint32_t    plane_slices_{ 1 };
            FORCE_INLINE bool operator==(const TextureDim& rhs)const {
                return !std::memcmp(this, &rhs, sizeof(*this));
            }
            FORCE_INLINE bool operator!=(const TextureDim& rhs)const {
                return !(*this == rhs);
            }
        };

        struct TextureSubRange {
            uint32_t    mip_{ 0 };
            uint32_t    layer_{ 0 };
            uint32_t    plane_{ 0 };
            bool operator==(const TextureSubRange& rhs)const {
                return mip_ == rhs.mip_ && layer_ == rhs.layer_ && plane_ == rhs.plane_;
            }
            bool operator!=(const TextureSubRange& rhs)const {
                return !(*this == rhs);
            }
        };
        struct TextureSubRangeRegion {
            uint32_t    base_mip_{ 0 };
            uint32_t    mips_{ 1 };
            uint32_t    base_layer_{ 0 };
            uint32_t    layers_{ 1 };
            uint32_t    base_plane_{ 0 };
            uint32_t    planes_{ 1 };
            explicit TextureSubRangeRegion(const TextureDim& layout): mips_(layout.mip_slices_),layers_(layout.array_slices_),
                                                                   planes_(layout.plane_slices_){}
            bool IsWholeRange(const TextureDim& layout)const{
                return base_mip_ == 0 && base_layer_ == 0
                    && base_plane_ == 0 && mips_ == layout.mip_slices_
                    && layers_ == layout.array_slices_ && planes_ == layout.plane_slices_;
            }
            uint32_t GetSubRangeIndexCount()const {
                return mips_ * layers_ * planes_;
            }
            bool IsSubRangeIndexIncluded(const TextureSubRange& index)const {
                if (index.mip_ < base_mip_ || index.mip_ > base_mip_ + mips_) {
                    return false;
                }
                if (index.layer_ < base_layer_ || index.layer_ > base_layer_ + layers_) {
                    return false;
                }
                if (index.plane_ < base_plane_ || index.plane_ > base_plane_ + planes_) {
                    return false;
                }
                return true;
            }
            uint32_t GetSubRangeIndexLocation(const TextureSubRange& index)const {
                if (!IsSubRangeIndexIncluded(index)) {
                    return -1;
                }
                return index.mip_ - base_mip_ + (index.layer_ - base_layer_) * mips_ +
                    (index.plane_ - base_plane_) * mips_ * planes_;
            }
            template<typename LAMBDA>
            requires std::is_invocable_v<LAMBDA, const TextureSubRange&>
            void For(LAMBDA&& lambda)const {
                for (auto pl = base_plane_; pl < base_plane_ + planes_; ++pl) {
                    for (auto lay = base_layer_; lay < base_layer_ + layers_; ++lay) {
                        for (auto mip = base_mip_; mip < base_mip_ + mips_; ++mip) {
                            lambda({mip, lay, pl});
                        }
                    }
                }
            }
        };

        struct BufferSubRangeRegion {
            uint32_t    offset_{ 0 };
            uint32_t    size_{ 0 };
            bool operator==(const BufferSubRange& rhs)const {
                return offset_ == rhs.offset_ &&
                    size_ == rhs.size_;
            }
            bool operator!=(const BufferSubRange& rhs)const {
                return !(*this != rhs);
            }
        };
        
        template<Utils::Integer T>
        struct HashName
        {
            T   name_;
            static HashName FromString(const String& name) {
                HashName hash_name{ static_cast<T>(CalcBlake3HashForBytes(name.data(), name.size())) }; //slow?
                return hash_name;
            }
            bool operator==(const HashName& rhs) const noexcept {
                return name_ == rhs.name_;
            }
            bool operator!=(const HashName& rhs) const noexcept {
                return !(*this == rhs);
            }
        };

        class Field
        {
        public:
            enum class EType : uint8_t
            {
                eBuffer,
                eTexture,
                eNum,
            };

            enum class EUsage :uint8_t
            {
                eUnkown    = 0x0,
                eInput    = 0x1,
                eOutput = 0x2,
                eExtracted    = eOutput | 0x10,
                eExternal = 0x100, //external resource
                eCulled    = 0x200, //field deleted
            };

            struct TextureSubField
            {
                EAccessFlags    access_;  
                /*for vulkan has so many layout type, and d3d12 RESOURCE_STATE not has layout,
                * so u can just leave layout_ as Unkown, and engine give it a value
                */
                ELayoutFlags    layout_{ ELayoutFlags::eUnkown };
            };
            using Name = HashName<uint32_t>;

            static bool IsSubFieldMergeAllowed(const TextureSubField& prev, const TextureSubField& next) {
                if (prev.access_ == next.access_ && prev.layout_ == next.layout_ ) {
                    return true;
                }
                auto union_access = Utils::LogicOrFlags(prev.access_, next.access_);
                if (Utils::HasAnyFlags(prev.access_, EAccessFlags::eReadOnly)
                    && Utils::HasAnyFlags(next.access_, EAccessFlags::eWriteAble)) {
                    return false;
                }
                if (Utils::HasAnyFlags(prev.access_, EAccessFlags::eWriteOnly)
                    && Utils::HasAnyFlags(next.access_, EAccessFlags::eReadAble)) {
                    return false;
                }
                //FIXME
                if (Utils::HasAnyFlags(union_access, EAccessFlags::eUAV) &&
                    Utils::HasAnyFlags(union_access, EAccessFlags::eNonUAV)) {
                    return false;
                }
                return true;
            }

            //fixme check pipeline outside function
            static bool IsSubFieldTransitionNeeded(const TextureSubField& prev, const TextureSubField& next) {
                if (prev.access_ != next.access_||IsSubFieldMergeAllowed(prev, next)) {
                    return true;
                }
                if (Utils::HasAnyFlags(next.access_, EAccessFlags::eUAV)) {
                    return true;
                }
                return false;
            }

            Field() = default;
            FORCE_INLINE bool IsWholeResource()const { return type_ == EType::eBuffer || texture_sub_range_.IsWholeRange(dims_); }
            FORCE_INLINE bool IsConnectAble(const Field& other)const;
            FORCE_INLINE bool IsExternal()const { return sub_resources_.access_ == EAccessFlags::eExternal; }
            FORCE_INLINE bool IsOutput()const { return Utils::HasAnyFlags(usage_, EUsage::eOutput); }
            FORCE_INLINE bool IsInput()const { return !IsOutput(); }
            FORCE_INLINE bool IsExtract()const { return usage_ == EUsage::eExtracted; }
            FORCE_INLINE bool IsReferenced()const { return ref_count_ > 1; }
            FORCE_INLINE bool IsCrossQueue()const { return is_cross_queue_; }
            FORCE_INLINE bool IsDedicated()const {}
            FORCE_INLINE bool IsTransiant()const{}
            Field& NameAs(const String& name);
            Field& ParentNameAs(const String& name);
            Field& Width(const uint32_t width);
            Field& Height(const uint32_t height);
            Field& Depth(const uint32_t depth);
            Field& MipSlices(const uint32_t mip_slices);
            Field& ArraySlices(const uint32_t array_slices);
            Field& PlaneSlices(const uint32_t plane_slices);
            Field& StageFlags(EPipelineStageFlags stage);
            //use with asyc compute and gfx queue
            Field& CrossQueue(bool value);
            Field& ForceTransiant();
            EType GetType()const;
            const String& GetName()const;
            const Name& GetHashName()const;
            const String& GetParentName()const;
            const TextureDim& GetDimension()const;
            const TextureSubField& GetSubField()const;
            const TextureSubRange& GetSubRange()const;
            EPixFormat GetPixFormat()const;
            uint32_t InCrRef(uint32_t count=1) { 
                ref_count_ += count;
                return ref_count_;
            }
            uint32_t DecrRef(uint32_t count=1) {
                ref_count_ -= count;
                return ref_count_;
            }
            friend bool operator==(const Field& lhs, const Field& rhs) {
                return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
            }
            friend bool operator!=(const Field& lhs, const Field& rhs) { return !(lhs == rhs); }
        private:
            friend class RenderPass;
            String  name_;
            Name    hash_name_;
            //name for producer
            Name    parent_name_;
            EType   type_;
            EUsage  usage_{ EUsage::eUnkown };
            EPixFormat  pix_fmt_{ EPixFormat::eUnkown };
            mutable uint32_t    ref_count_{ 1 };
            //which stage of pipeline use this Field
            EPipelineStageFlags stage_{ EPipelineStageFlags::eAll };
            //user force it tobe transiant
            uint32_t    is_dedicated_:1;
            uint32_t    is_transiant_:1;
            uint32_t    is_cross_queue_:1;
            uint32_t    sample_count_{ 1u };
            ETextureDimType dim_prop_{ ETextureDimType::eAbsolute };
            TextureDim  dims_;
            union {
                /*texture range current field used*/
                TextureSubRangeRegion texture_sub_range_;
                BufferSubRangeRegion  buffer_sub_range_;
            };
            TextureSubField sub_resources_;
        };

        /*parent resource class for texture and buffer*/
        class RenderResource
        {
        public:
            using ThisType = RenderResource;

            RenderResource() = default;
            RenderResource(const Field& field) { Init(field); }
            virtual void Init(const Field& field);
            virtual ~RenderResource() {}
            bool IsExternal()const {
                return is_external_;
            }
            bool IsTransient()const {
                return is_transient_;
            }
            bool IsOutput()const {
                return is_output_;
            }
            void Regist(uint32_t pass) {
                life_time_.first = std::min(life_time_.first, pass);
                life_time_.second = std::max(life_time_.second, pass);
            }
            bool IsValid(uint32_t pass) const {
                return pass >= life_time_.first && pass <= life_time_.second;
            }
            void IncrRef(uint32_t ref) const{
                ref_count_.fetch_add(ref);
            }
            void DecrRef(uint32_t ref) const{
                ref_count_.fetch_sub(ref);
                assert(ref_count_ >= 0);
            }
            auto& GetAccessFlags() {
                return access_flags_;
            }
            auto& GetLayoutFlags() {
                return layout_flags_;
            }
            /*set rhi backend for this resource*/
            virtual void SetRHI() = 0;
            /*release rhi to the backend*/
            virtual void EndRHI() = 0;
        private:
            uint32_t    is_external_:1{ 0u };
            uint32_t    is_transient_:1{ 1u };
            uint32_t    is_output_ : 1{ 0u };
            EAccessFlags    access_flags_{ EAccessFlags::eNone };
            ELayoutFlags    layout_flags_{ ELayoutFlags::eUnkown };
            mutable std::atomic_uint    ref_count_{ 0u };
            std::pair<uint32_t, uint32_t>   life_time_{ 0u, 0u };
        };

        class RenderBuffer : public RenderResource
        {
        public:
            using BufferRepo = Utils::ResourceManager<RenderBuffer, Utils::ScalablePoolAllocator>;
            using Handle = BufferRepo::Handle;
            static BufferRepo& BufferRepoInstance();
        public:
            void SetRHI()override;
            void EndRHI()override;
        private:
            RHI::RHIBuffer* rhi_buffer_{ nullptr };
        };

        using TextureSubRangeState = TextureSubFieldState;

        class RenderTexture : public RenderResource
        {
        public:
            using TextureRepo = Utils::ResourceManager<RenderTexture, Utils::ScalablePoolAllocator>;
            using Handle = TextureRepo::Handle;
            /*texture and buffer repo singleton instance*/
            static TextureRepo& TextureRepoInstance();
        public:
            void SetRHI()override;
            void EndRHI()override;
            void MergeField(const Field& curr_field);
        private:
            SmallVector<TextureSubRangeState>   curr_state_;
            
            RHI::RHITexture*    rhi_texture_{ nullptr };
        };

        static constexpr uint32_t MAX_RENDER_TARGET_ATTACHMENTS = 8;

        struct ALIGN_AS(128) RenderFrameBuffer
        {
            RenderTexture    depth_stencil_;
            RenderTexture    color_attachments_[MAX_RENDER_TARGET_ATTACHMENTS];
        };

    }
}
