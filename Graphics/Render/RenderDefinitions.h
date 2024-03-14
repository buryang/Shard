#pragma once

#include "Utils/CommonUtils.h"

namespace Shard
{
    namespace Render
    {
        //for vulkan barrier need stage information
        enum class EPipelineStageFlags
        {
            eTopOfPipe = 1 << 0,
            eDrawIndirect = 1 << 1,
            eVertexInput = 1 << 2,
            eShaderVertex = 1 << 3,
            eShaderTControl = 1 << 4,
            eShaderTEval = 1 << 5,
            eShaderGeometry = 1 << 6,
            eShaderFrag = 1 << 7,
            eShaderCompute = 1 << 8,
            eAllGFX = 1 << 9,
            eAllCommand = 1 << 10,
            eBuildAS = 1 << 11,
            eRayTrace = 1 << 12,
            eBottomOfPipe = 1 << 13,
            eAll = ~0u,
        };

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
            eNone = 0,
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
            eDSV = 1 << 11,
            ePresent = 1 << 12,
            eExternal = 1 << 13,
            eReadOnly = eVertexBuffer | eIndexBuffer | eIndirectArgs | eTransferSrc | eSRV | eDSVRead,
            eReadAble = eReadOnly | eUAV,
            eWriteOnly = eRTV | eTransferDst | eDSVWrite,
            eWriteAble = eWriteOnly | eUAV,
            eNonUAV = ~eUAV,
        };

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
    }
}