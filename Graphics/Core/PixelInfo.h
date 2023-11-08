#pragma once
#include "Utils/CommonUtils.h"

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

	/*texture clear value*/
	struct PixClearValue {
		struct DSV
		{
			uint32_t	stencil_;
			float	depth_;
		};
		union {
			float	color_[4];
			DSV	depth_stencil_;
		};
	};

	//clear value predefine
	static constexpr PixClearValue CLEAR_VALUE_BLACK{ .color_ = {0.f, 0.f, 0.f, 1.f} };
	static constexpr PixClearValue CLEAR_VALUE_WHITE{ .color_ = {1.f, 1.f, 1.f, 1.f} };
	static constexpr PixClearValue CLEAR_VALUE_ZERO{ .color_ = {0.f, 0.f, 0.f, 0.f} };//fixme
	static constexpr PixClearValue CLEAR_VALUE_DEPTH{ .depth_stencil_ = {.depth_ = 0.f} };
	static constexpr PixClearValue CLEAR_VALUE_STENCIL{ .depth_stencil_ = {.stencil_ = 0} };
}

