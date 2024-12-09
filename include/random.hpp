#pragma once

#include <cstdint>

namespace rg
{
    struct XorShift
    {
        using result_type = uint32_t;
        uint32_t state;

        explicit XorShift(uint32_t seed) : state(seed)
        {
            if(state == 0)
            {
                state = 0xdead'beef; // Ensure the state is non-zero
            }
        }

        uint32_t operator()()
        {
            uint32_t x = state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state = x;
            return x;
        }

        static constexpr uint32_t min()
        {
            return 0;
        }

        static constexpr uint32_t max()
        {
            return UINT32_MAX;
        }
    };
} // namespace rg
