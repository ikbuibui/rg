#pragma once

#include <cstdint>

namespace rg
{
    // Sentinel for the invalid waiter counter state during registration
    static constexpr uint16_t INVALID_WAIT_STATE = 1u << 15;
} // namespace rg
