
#pragma once

#include "alloc/MemBlk.hpp"

#include <cstddef>

namespace rg
{

    template<size_t Threshold, class SmallAllocator, class LargeAllocator>
    class Segregator
    {
    public:
        // static constexpr unsigned alignment = (SmallAllocator::alignment > LargeAllocator::alignment)
        //                                           ? SmallAllocator::alignment
        //                                           : LargeAllocator::alignment;

        static MemBlk allocate(size_t n) noexcept
        {
            if(n <= Threshold)
            {
                return SmallAllocator::allocate(n);
            }
            else
            {
                return LargeAllocator::allocate(n);
            }
        }

        /**
         * Frees the given block and resets it.
         * \param b The block to be freed.
         */
        static void deallocate(MemBlk b) noexcept
        {
            if(b.n <= Threshold)
            {
                SmallAllocator::deallocate(b);
            }
            else
            {
                LargeAllocator::deallocate(b);
            }
        }
    };
} // namespace rg
