#pragma once
#include <variant>
#include "Utils/CommonUtils.h"

/**
 * base abstract on vulkan and D3D12EnhancedBarriers
 * follow https://anki3d.org/simplified-pipeline-barriers/ to simplify state
 */

namespace Shard {
    //pixformat
    enum class EPixFormat : uint16_t
    {
        eUnkown,
        eR8Unorm,
        eR8Snorm,
        eR16Unorm,
        eR16Snorm,
        eRG8Unorm,
        eRG8Snorm,
        eRG16Unorm,
        eRG16Snorm,
        eRGB16Unorm,
        eRGB16Snorm,
        eR24UnormX8,
        eRGB5A1Unorm,
        eRGBA8Unorm,
        eRGBA8Snorm,
        eRGB10A2Unorm,
        eRGB10A2Uint,
        eRGBA16Unorm,
        eRGBA8UnormSrgb,
        eR16Float,
        eRG16Float,
        eRGB16Float,
        eRGBA16Float,
        eR32Float,
        eR32FloatX32,
        eRG32Float,
        eRGB32Float,
        eRGBA32Float,
        eR11G11B10Float,
        eRGB9E5Float,
        eR8Int,
        eR8Uint,
        eR16Int,
        eR16Uint,
        eR32Int,
        eR32Uint,
        eRG8Int,
        eRG8Uint,
        eRG16Int,
        eRG16Uint,
        eRG32Int,
        eRG32Uint,
        eRGB16Int,
        eRGB16Uint,
        eRGB32Int,
        eRGB32Uint,
        eRGBA8Int,
        eRGBA8Uint,
        eRGBA16Int,
        eRGBA16Uint,
        eRGBA32Int,
        eRGBA32Uint,

        eBGRA8Unorm,
        eBGRA8UnormSrgb,

        eBGRX8Unorm,
        eBGRX8UnormSrgb,
        eAlpha8Unorm,
        eAlpha32Float,
        eR5G6B5Unorm,

        // Depth-stencil
        eD32Float,
        eD16Unorm,
        eD32FloatS8X24,
        eD24UnormS8,

        // Compressed formats
        eBC1Unorm,   // DXT1
        eBC1UnormSrgb,
        eBC2Unorm,   // DXT3
        eBC2UnormSrgb,
        eBC3Unorm,   // DXT5
        eBC3UnormSrgb,
        eBC4Unorm,   // RGTC Unsigned Red
        eBC4Snorm,   // RGTC Signed Red
        eBC5Unorm,   // RGTC Unsigned RG
        eBC5Snorm,   // RGTC Signed RG
        //The BC6H format provides high-quality compression for images that use three HDR color
        //channels, with a 16-bit float value for each color channel of the value (16:16:16). 4x4 tile 
        eBC6HS16,    
        eBC6HU16,
        //better-than-average quality compression with less artifacts for standard RGB source data.4x4 tile
        eBC7Unorm,
        eBC7UnormSrgb,
        eNum,
    };

    /**texture clear value*/
    struct PixClearValue {
        struct DSV
        {
            uint32_t    stencil_{ 0u };
            float       depth_{ 0.f };
        };
        std::variant<vec4, DSV> val_;
    };

    /*
     *color more easily handles opaque or transparent black or white, so use {1.0, 0.0, 0.0, 0.0},
     *{0.0, 1.0, 1.0, 1.0}, or all 0.0s or 1.0s for the clear color of ARGB surfaces.
     *Any combination of 1.0s and 0.0s works best for two channel formats
     */
    static constexpr PixClearValue CLEAR_VALUE_BLACK{ .val_ = vec4{0.f, 0.f, 0.f, 1.f} }; //RGBA black
    static constexpr PixClearValue CLEAR_VALUE_WHITE{ .val_ = vec4{1.f, 1.f, 1.f, 1.f} }; //RGBA white
    static constexpr PixClearValue CLEAR_VALUE_GRAY{ .val_ = vec4{0.5f, 0.5f, 0.5f, 1.f} }; //RGBA gray
    static constexpr PixClearValue CLEAR_VALUE_ZEROS{ .val_ = vec4{0.f, 0.f, 0.f, 0.f} };
    static constexpr PixClearValue CLEAR_VALUE_ONES{ .val_ = vec4{1.f, 1.f, 1.f, 1.f} };
    static constexpr PixClearValue CLEAR_VALUE_DEPTH_STENCIL{ .val_ = PixClearValue::DSV{} };

    //for vulkan barrier need stage information
    enum class EPipelineStageFlags : uint32_t
    {
        eTopOfPipe = 1u << 0,
        eDrawIndirect = 1u << 1,
        eVertexInput = 1u << 2,
        eShaderVertex = 1u << 3,
        eShaderTControl = 1u << 4,
        eShaderTEval = 1u << 5,
        eShaderGeometry = 1u << 6,
        eShaderFrag = 1u << 7,
        eShaderCompute = 1u << 8,
        eTransfer = 1u << 9,
        eBuildAS = 1u << 10,
        eRayTrace = 1u << 11,
        eBottomOfPipe = 1 << 12,
        eAllGFX = ~(eBuildAS | eRayTrace),
        eAllCompute = eTopOfPipe | eDrawIndirect | eShaderCompute | eTransfer | eBottomOfPipe, 
        eAll = ~0u,
    };
    DECLARE_ENUM_BIT_OPS(EPipelineStageFlags);

    enum class EPipeline : uint8_t
    {
        eNone = 0x0,
        eGFX = 0x1,
        eAsyncCompute = 0x2,
        eAll = eGFX | eAsyncCompute,
        eNum = 0x3,
    };

    //resource access/usage flags
    enum class EAccessFlags : uint32_t
    {
        eUndefined = 0u, //may d3d12 set it to COMMON?
        eIndirectArgs = 1 << 0,
        eIndexBuffer = 1 << 1,
        eVertexBuffer = 1 << 2,
        eTransferSrc = 1 << 3,
        eTransferDst = 1 << 4,
        eDSVRead = 1 << 5,
        eDSVWrite = 1 << 6,
        eSRV = 1 << 8,
        eRTV = 1 << 9,
        eUAV = 1 << 10,
        eDSV = eDSVRead | eDSVWrite,
        ePresent = 1 << 12,
        eExternal = 1 << 13,
        eRayTraceAS = 1 << 14,
        eReadOnly = eVertexBuffer | eIndexBuffer | eIndirectArgs | eTransferSrc | eSRV | eDSVRead,
        eReadAble = eReadOnly | eUAV | eRayTraceAS,
        eWriteOnly = eRTV | eTransferDst | eDSVWrite,
        eWriteAble = eWriteOnly | eUAV | eRayTraceAS,
        eNonUAV = ~eUAV,
    };
    DECLARE_ENUM_BIT_OPS(EAccessFlags);

    static inline bool IsReadOnly(EAccessFlags flags) {
        return !Utils::HasAnyFlags(flags, EAccessFlags::eReadOnly);
    }
    static inline bool IsReadAble(EAccessFlags flags) {
        return Utils::HasAnyFlags(flags, EAccessFlags::eReadAble);
    }
    static inline bool IsWriteOnly(EAccessFlags flags) {
        return !Utils::HasAnyFlags(flags, EAccessFlags::eWriteOnly);
    }
    static inline bool IsWriteAble(EAccessFlags flags) {
        return Utils::HasAnyFlags(flags, EAccessFlags::eWriteAble);
    }
    static inline bool IsAccessFlagsUsageSupportedByGFXQueue(EAccessFlags flags) { return true; }
    static inline bool IsAccessFlagsTransitionSupportedByGFXQueue(EAccessFlags flags) { return true; }
    static inline bool IsAccessFlagsUsageSupportedByComputeQueue(EAccessFlags flags) {
        const auto allowed_flags = EAccessFlags::eRayTraceAS | EAccessFlags::eTransferSrc
                                | EAccessFlags::eTransferDst | EAccessFlags::eUAV | EAccessFlags::eDSVRead;
        return !Utils::HasAnyFlags(~allowed_flags, flags);
    }
    static inline bool IsAccessFlagsTransitionSupportedByComputeQueue(EAccessFlags flags) {
        const auto allowed_flags = EAccessFlags::eRayTraceAS | EAccessFlags::eTransferSrc |
            EAccessFlags::eTransferDst | EAccessFlags::eUAV;
        return !Utils::HasAnyFlags(flags, ~allowed_flags);
    }
    static inline bool IsAccessFlagsUsageSupportedByTransferQueue(EAccessFlags flags) {
        const auto allowed_flags = EAccessFlags::eTransferDst | EAccessFlags::eTransferSrc;
        return !Utils::HasAnyFlags(flags, allowed_flags);
    }
    static inline bool IsAccessFlagsTransitionSupportedByTransferQueue(EAccessFlags flags) {
        return IsAccessFlagsUsageSupportedByComputeQueue(flags);
    }
    enum class ELayoutFlags
    {
        eUnkown,
        eLinear, //row major
        eOptimal, //d3d12 swizzle, vulkan has so much optimal decided by accessflags
    };

    enum class ETextureDimType
    {
        eAbsolute,
        eSwapchainRelative,
        eInputRelative,
    };

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
        FORCE_INLINE static uint32_t MipLevels(const TextureDim& dim) {
            const auto max_dim = std::max(dim.width_, std::max(dim.height_, dim.depth_));
            return Utils::BitScanMSB(max_dim);
        }
    };

    struct TextureSubRangeIndex {
        uint32_t    mip_{ 0 };
        uint32_t    layer_{ 0 };
        uint32_t    plane_{ 0 };
        bool operator==(const TextureSubRangeIndex& rhs)const {
            return mip_ == rhs.mip_ && layer_ == rhs.layer_ && plane_ == rhs.plane_;
        }
        bool operator!=(const TextureSubRangeIndex& rhs)const {
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

        static TextureSubRangeRegion FromDims(const TextureDim& dim) {
            TextureSubRangeRegion region{
                .mips_ = dim.mip_slices_,
                .layers_ = dim.array_slices_,
                .planes_ = dim.plane_slices_
            };
            return region;
        }
        bool IsWholeRange(const TextureDim& layout)const {
            return base_mip_ == 0 && base_layer_ == 0
                && base_plane_ == 0 && mips_ == layout.mip_slices_
                && layers_ == layout.array_slices_ && planes_ == layout.plane_slices_;
        }
        uint32_t GetSubRangeIndexCount()const {
            return mips_ * layers_ * planes_;
        }
        bool IsSubRangeIndexIncluded(const TextureSubRangeIndex& index)const {
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
        uint32_t GetSubRangeIndexLocation(const TextureSubRangeIndex& index)const {
            if (!IsSubRangeIndexIncluded(index)) {
                return -1;
            }
            return index.mip_ - base_mip_ + (index.layer_ - base_layer_) * mips_ +
                (index.plane_ - base_plane_) * mips_ * planes_;
        }
        template<typename LAMBDA>
            requires std::is_invocable_v<LAMBDA, const TextureSubRangeIndex&>
        void Enumerate(LAMBDA&& lambda)const {
            for (auto pl = base_plane_; pl < base_plane_ + planes_; ++pl) {
                for (auto lay = base_layer_; lay < base_layer_ + layers_; ++lay) {
                    for (auto mip = base_mip_; mip < base_mip_ + mips_; ++mip) {
                        lambda({ mip, lay, pl });
                    }
                }
            }
        }
    };

    FORCE_INLINE bool operator==(const TextureSubRangeRegion& lhs, const TextureSubRangeRegion& rhs) {
        return lhs.base_mip_ == rhs.base_mip_ && lhs.mips_ == rhs.mips_ &&
            lhs.base_layer_ == rhs.base_layer_ && lhs.layers_ == rhs.layers_ &&
            lhs.base_plane_ == rhs.base_plane_ && lhs.planes_ == rhs.planes_;
    }

    FORCE_INLINE bool operator!=(const TextureSubRangeRegion& lhs, const TextureSubRangeRegion& rhs) {
        return !(lhs == rhs);
    }

    struct BufferSubRangeRegion {
        uint32_t    offset_{ 0 };
        uint32_t    size_{ 0 };
        bool operator==(const BufferSubRangeRegion& rhs)const {
            return offset_ == rhs.offset_ &&
                size_ == rhs.size_;
        }
        bool operator!=(const BufferSubRangeRegion& rhs)const {
            return !(*this != rhs);
        }
    };

    struct TextureState
    {
        EAccessFlags    access_;
        /*for vulkan has so many layout type, and d3d12 RESOURCE_STATE not has layout,
        * so u can just leave layout_ as Unkown, and engine give it a value
        */
        ELayoutFlags    layout_{ ELayoutFlags::eUnkown };
    };

    static inline bool IsTextureStateMergeAllowed(const TextureState& prev, const TextureState& next) {
        if (prev.access_ == next.access_ && prev.layout_ == next.layout_) {
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
    static inline bool IsTextureStateTransitionNeeded(const TextureState& prev, const TextureState& next) {
        if (prev.access_ != next.access_ || IsTextureStateMergeAllowed(prev, next)) {
            return true;
        }
        if (Utils::HasAnyFlags(next.access_, EAccessFlags::eUAV)) {
            return true;
        }
        return false;
    }

    static inline TextureState MergeState(const TextureState& prev, const TextureState& next) {
        TextureState new_state{ .access_ = prev.access_ | next.access_, .layout_ = prev.layout_ };
        return new_state;
    }
}

