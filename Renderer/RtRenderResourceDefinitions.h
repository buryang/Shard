#pragma once

#include "Utils/CommonUtils.h"

namespace MetaInit
{
	namespace Renderer
	{
		enum class BufferFlags :uint8_t {
			eUnkown = 0x0,
			eRaw = 0x1,
			eIndex = 0x2,
			eDrawIndirect = 0x3,
			eVertex = 0x4,
			eStructed = 0x3,
			eUniform = 0x4,
			eStorage = 0x5,
			eDynamic = 0x8,
			eCopySrc = 0x9,
			eShaderBinding = 0x10,
			eAccelerationStruct = 0x11,
		};

		struct RtBufferCreateDesc
		{
			using Flags = BufferFlags;
			Flags	flags_{ Flags::eUnkown };
			size_t	size_;
			//stride in bytes
			uint32_t	stride_{ 0 };
			union {
				struct {

				};
				uint32_t	pack_bits_{ 0 };
			};

			bool operator==(const RtBufferCreateDesc& rhs)const {
				return flags_ == rhs.flags_ && size_ == rhs.size_;
			}
			bool operator!=(const RtBufferCreateDesc& rhs)const {
				return !(*this == rhs);
			}
		};

		//front for vulkan image dx texture desc
		struct RtTextureCreateDesc
		{
			PixelFormat	format_;
			vec3		extent_;
			uint32_t	array_size_;
			uint32_t	flags_;
			uint32_t	mips_;
			uint32_t	samples_;
			union {
				struct
				{
					uint32_t	is_cube_map_ : 1;
				};
				uint32_t pack_bits_{ 0 };
			};

			bool IsCubeMap() const {
				return is_cube_map_;
			}

			bool IsTexture2D() const {
				return extent_.x != 0 && extent_.y != 0 && !is_cube_map_ && !extent_.z;
			}
			
			bool IsTexture3D() const {
				return extent_.x != 0 && extent_.y != 0 && !is_cube_map_ && extent_.z != 0;
			}
			bool operator==(const RtTextureCreateDesc& rhs)const {

			}
			bool operator!=(const RtTextureCreateDesc& rhs)const {
				return !(*this == rhs);
			}
		};

		enum class PipelineStageFlags : uint8_t
		{
			eStageUnkown = 0x0,
			eStageVertex = 0x1,
			eStageGemotry = 0x2,
			eStageDomain = 0x3,
			eStagePixel = 0x4,
		};
	}
}