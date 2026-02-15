#pragma once

#include "Utils/CommonUtils.h"

namespace Shard::Utils
{
    /* Halton: "Radical-inverse quasi-random point sequence"
     * return halton sequence value for given index and base(2 or 3)
     */
    [[nodiscard]] constexpr auto Halton(int32_t index, int32_t base)
    {
        float r = 0.0f;
        float f = 1.0f;
        uint32_t i = index;

        while (i > 0)
        {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(i % base);
            i /= base;
        }

        return r;
    }

    //blue noise and other ones

}
