
#pragma once

#include "alloc/MemBlk.hpp"
#include "waitCounter.hpp"

#include <cstddef>

namespace rg
{

    template<typename BaseAllocator>
    struct AlignedAllocator
    {
        // Align size to prevent false sharing
        static constexpr std::size_t align_size(std::size_t n) noexcept
        {
            // TODO is is better to & with -hardware_destructive_interference_size;
            return (n + hardware_destructive_interference_size - 1u) & ~(hardware_destructive_interference_size - 1u);
        }

        // Allocate memory using the base allocator
        static MemBlk allocate(std::size_t n)
        {
            auto blk = BaseAllocator::allocate(align_size(n));
            return {blk.ptr, n};
        }

        // Deallocate memory using the base allocator
        static void deallocate(MemBlk blk)
        {
            BaseAllocator::deallocate({blk.ptr, align_size(blk.n)});
        }
    };
} // namespace rg
