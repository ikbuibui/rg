#pragma once

#include "ThreadPool.hpp"

#include <concepts>

namespace rg
{

    // TODO NOEXCEPT and exception safety
    // holds the pool
    struct rg2
    {
        rg2(std::unsigned_integral auto size) : pool{size}
        {
        }

        ThreadPool* pool_ptr()
        {
            return &pool;
        }

    private:
        ThreadPool pool;
    };

    auto init(std::unsigned_integral auto size) -> rg2
    {
        return {size};
    }
} // namespace rg
