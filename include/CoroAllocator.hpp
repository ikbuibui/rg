#pragma once

#include "alloc/AlignedAllocator.hpp"
#include "alloc/FreeList.hpp"
#include "alloc/FreeListTLS.hpp"
#include "alloc/OpNewAllocator.hpp"
#include "alloc/Segregator.hpp"
#include "alloc/SlabTLS.hpp"

#include <cstddef>

namespace rg
{

    // Special purpose allocators for coroutine frames
    // Inspired by Andrei Alexandrescu's talk on allocators and heap layers

    using CoroAllocator
        = Segregator<894, SlabTLSAllocator<FreeListTLS<OpNewAllocator, 1024 * 8>, 894, 8>, OpNewAllocator>;

} // namespace rg
