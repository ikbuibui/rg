#pragma once

#include "alloc/AlignedAllocator.hpp"
#include "alloc/OpNewAllocator.hpp"

namespace rg
{

    // Special purpose allocators for coroutine frames
    // Inspired by Andrei Alexandrescu's talk on allocators and heap layers

    using CoroAllocator = AlignedAllocator<OpNewAllocator>;
    // = Segregator<894, SlabTLSAllocator<FreeListTLS<OpNewAllocator, 1024 * 8>, 894, 8>, OpNewAllocator>;

} // namespace rg
