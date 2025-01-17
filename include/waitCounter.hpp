#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace rg
{

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    inline constexpr std::size_t hardware_destructive_interference_size = 2 * sizeof(std::max_align_t);
#endif

    // Sentinel for the invalid waiter counter state during registration
    static constexpr uint32_t INVALID_WAIT_STATE = 1u << 31;
    using TWaitCount = uint32_t;
} // namespace rg
