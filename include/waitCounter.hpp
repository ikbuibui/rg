#pragma once

#include <cstdint>

namespace rg
{
    // Sentinel for the invalid waiter counter state during registration
    static constexpr uint32_t INVALID_WAIT_STATE = 1u << 31;
} // namespace rg
