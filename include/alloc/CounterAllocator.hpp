#pragma once

#include "alloc/MemBlk.hpp"

#include <atomic>
#include <cstddef>
#include <iostream>

namespace rg
{
    template<class BaseAllocator>
    class CounterAllocator
    {
    public:
        static inline std::atomic<size_t> allocation_count{0};

        static MemBlk allocate(size_t n)
        {
            ++allocation_count;
            // std::cout << "size allocated " << n << std::endl;
            return BaseAllocator::allocate(n);
        }

        static void deallocate(MemBlk blk)
        {
            BaseAllocator::deallocate(blk);
        }

        static std::size_t get_allocation_count()
        {
            return allocation_count.load();
        }
    };
} // namespace rg
