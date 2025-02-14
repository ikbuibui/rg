#pragma once

#include "alloc/AlignedAllocator.hpp"
#include "alloc/CounterAllocator.hpp"
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

    using CoroAllocator = AlignedAllocator<Segregator<1024, FreeListTLS<OpNewAllocator, 1024>, OpNewAllocator>>;

} // namespace rg
