
#pragma once

#include "alloc/MemBlk.hpp"

#include <cstddef>

namespace rg
{

    // allocates memory using the global new operator
    struct OpNewAllocator
    {
        static MemBlk allocate(std::size_t n)
        {
            return MemBlk(::operator new(n), n);
        }

        static void deallocate(MemBlk blk)
        {
            ::operator delete(blk.ptr, blk.n);
        }
    };
} // namespace rg
