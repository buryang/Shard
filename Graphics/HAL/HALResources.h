#pragma once
#include "Utils/Handle.h"
#include "Core/EngineGlobalParams.h"
#include "Graphics/Render/RenderResources.h"
#include "Graphics/HAL/HALCommonUtils.h"
#include "Graphics/HAL/HALGlobalEntity.h"

namespace Shard
{
    namespace HAL
    {
        //resource global parameter configure
        //whether resource shade mode be concurrent 
        REGIST_PARAM_TYPE(BOOL, HAL_CONCURRENT_SHARE, false);

        class HALResource
        {
        public:
            HALResource(HALGlobalEntity* parent, const String& name):parent_(parent), name_(name) {}
            DISALLOW_COPY_AND_ASSIGN(HALResource);
            virtual void SetUp() = 0;
            virtual void Release() = 0;
            virtual size_t GetOccupySize() const = 0;
            virtual ~HALResource();
            FORCE_INLINE bool IsDedicated() const { 
                return is_dedicated_;
            }
            FORCE_INLINE bool IsTransient() const {
                return is_transient_;
            }
            FORCE_INLINE bool IsBinded() const {
                return bind_mem_ != nullptr;
            }
            FORCE_INLINE uint32_t LifeBegin() const {
                return life_time_.start_;
            }
            FORCE_INLINE uint32_t LifeEnd() const {
                return life_time_.end_;
            }
            //todo support bind&alias interface
            FORCE_INLINE HALAllocation*& GetBindMemory(){
                return bind_mem_;
            }
            OVERLOAD_OPERATOR_NEW(HALResource);
        protected:
            uint32_t AddRef() {
                return ++ref_count_;
            }
            uint32_t DecrRef(uint32_t count) {
                assert(ref_count_ > count);
                ref_count_ -= count;
                return ref_count_;
            }
            uint32_t RefCount() const {
                return ref_count_;
            }
        protected:
            //no multithread 
            mutable uint32_t    ref_count_{ 0 };
            HALGlobalEntity*    parent_{ nullptr };
            String  name_;
            uint32_t    is_dedicated_ : 1;
            uint32_t    is_transient_ : 1;
            uint32_t    is_alias_ : 1;
            struct
            {
                uint16_t    start_{ 0u };
                uint16_t    end_{ 0u };
            }life_time_;
            HALAllocation* bind_mem_{ nullptr };
            //bind to alias memory, does not response to alloc/release allocation
            HALAllocation* alias_mem_{ nullptr };
        };
        class HALBuffer;
        class HALTexture;
        class HALBufferView;
        class HALTextureView;


        struct HALTextureInitializer
        {
            using HashType = Utils::SpookyV2Hash32;
            enum class EType : uint8_t
            {
                eTexture1D = 0x0,
                eTexture2D = 0x1,
                eTexture3D = 0x2,
                eTextureCube = 0x3,
                eTextureArray = 0x4,
            };
            enum class EAspect : uint8_t
            {
                eUnkown = 0,
                eColor  = 1 << 1,
                eDepth  = 1 << 2,
                eStencil    = 1 << 3,
                eMeta   = 1 << 4,
                ePlane0 = 1 << 5,
                ePlane1 = 1 << 6,
                ePlane2 = 1 << 7,
            };
            HashType Hash()const {
                return Utils::InternSpookyV2HashForBytes<32>(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
            }
            EType   type_;
            TextureDim  dims_;
            EAccessFlags    access_{ EAccessFlags::eNone };
            ELayoutFlags    layout_{ ELayoutFlags::eLinear };
            EPipeline   pipeline_{ EPipeline::eGFX };
            EPixFormat  format_{ EPixFormat::eUnkown };
        };

        FORCE_INLINE HALTextureInitializer::EType GetTextureType(const HALTextureInitializer& desc) {
            assert(desc.dims_.width_ != 0 && "at least one dimension nonzero");
            if (desc.dims_.array_slices_ != 0) {
                return  HALTextureInitializer::EType::eTextureArray;
            }
            if (desc.dims_.depth_ != 0) {
                return HALTextureInitializer::EType::eTexture3D;
            }
            if (desc.dims_.width_ != 0 && desc.dims_.height_ != 0) {
                return  HALTextureInitializer::EType::eTexture2D;
            }
            return  HALTextureInitializer::EType::eTexture1D;
        }
        //FIXME
        FORCE_INLINE bool operator==(const HALTextureInitializer& lhs, const HALTextureInitializer& rhs) {
            return lhs.dims_ == rhs.dims_ && lhs.format_ == rhs.format_
                && lhs.access_ == rhs.access_;
        }

        FORCE_INLINE bool operator!=(const HALTextureInitializer& lhs, const HALTextureInitializer& rhs) {
            return !(lhs == rhs);
        }

        struct HALTextureViewInitializer
        {
            using HashType = Utils::SpookyV2Hash32;
            HashType Hash()const
            {
                return Utils::InternSpookyV2HashForBytes<32>(reinterpret_cast<const uint8_t*>(this), sizeof(*this));
            }
            //HALTexture* texture_{ nullptr };
            TextureSubRangeRegion region_;
            EPixFormat  format_{ EPixFormat::eUnkown };
        };

        FORCE_INLINE bool operator==(const HALTextureViewInitializer& lhs, const HALTextureViewInitializer& rhs) {
            return /*lhs.texture_ == rhs.texture_*/ && 0;
        }

        FORCE_INLINE bool operator!=(const HALTextureViewInitializer& lhs, const HALTextureViewInitializer& rhs) {
            return !(lhs == rhs);
        }

        class HALTexture : public HALResource
        {
        public:
            explicit HALTexture(HALGlobalEntity* parent, const HALTextureInitializer& desc, const String& texture_name) :HALResource(parent, texture_name), desc_(desc) {}
            virtual ~HALTexture() {}
            FORCE_INLINE const HALTextureInitializer& GetTextureDesc() const {
                return desc_;
            }
            HALTextureView* GetOrCreateTextureView(const HALTextureViewInitializer& view_initializer);
        protected:
            virtual HALTextureView* CreateTextureViewImpl(const HALTextureViewInitializer& view_initializer) = 0;
        protected:
            const HALTextureInitializer& desc_;
            Map<HALTextureViewInitializer::HashType, HALTextureView*> views_;
        };

        struct HALBufferInitializer
        {
            enum class EType : uint8_t
            {
                eUniform = 0x1,
                eReadWrite = 0x2,
                eRawBuffer = 0x4,
                eStructedBuffer = 0x8,
                eUniformStructed = eUniform | eStructedBuffer,
                eRWStructed = eReadWrite | eStructedBuffer,
            };

            EType       type_;
            uint32_t    size_;
            uint32_t    bytes_stride_;//todo element size for buffer
            EPipeline   pipeline_{ EPipeline::eGFX };
            EAccessFlags    access_{ EAccessFlags::eNone };
            uint32_t    uav_counter_ : 1{0u};
            uint32_t    append_ : 1{0u};
        };

        FORCE_INLINE bool operator==(const HALBufferInitializer& lhs, const HALBufferInitializer& rhs) {
            return lhs.type_ == rhs.type_ && lhs.size_ == rhs.size_
                && lhs.pipeline_ == rhs.pipeline_
                && lhs.access_ == rhs.access_;
        }
        FORCE_INLINE bool operator!=(const HALBufferInitializer& lhs, const HALBufferInitializer& rhs) {
            return !(lhs == rhs);
        }

        class HALBuffer : public HALResource
        {
        public:
            explicit HALBuffer(HALGlobalEntity* parent, const HALBufferInitializer& desc, const String& buffer_name) :HALResource(parent, name), desc_(desc) {}
            virtual ~HALBuffer() {};
            FORCE_INLINE const HALBufferInitializer& GetBufferDesc() const {
                return desc_;
            }
            size_t GetOccupySize() const override;
            HALBuffer* GetUAVCounter() const {
                if (desc_.uav_counter_) {
                    assert(uav_counter_ != nullptr);
                    return uav_counter_;
                }
                return nullptr;
            }
        protected:
            const HALBufferInitializer& desc_;
            //simulated uav counter
            HALBuffer* uav_counter_{ nullptr };
            Map<>
        };

        struct HALAccelerateDesc
        {

        };

        class HALAccelerate : public HALResource
        {
        public:
            using SharedPtr = eastl::shared_ptr<HALAccelerate>;
            enum class Type : uint8_t
            {
                eTopLevel,
                eBottomLevel,
                eNum,
            };
            virtual ~HALAccelerate() {}
            OVERLOAD_OPERATOR_NEW(HALAccelerate);
        private:
            const HALAccelerateDesc& desc_;
        };

        class HALSampler : public HALResource
        {
        public:
            using Ptr = HALSampler*;
            virtual ~HALSampler() {}
        };
    }
}

//todo texture compression https://www.ludicon.com/castano/blog/2025/02/gpu-texture-compression-everywhere/
