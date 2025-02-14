
#pragma once

#include "alloc/MemBlk.hpp"
#include "cdrc_stack.hpp"

#include <boost/lockfree/stack.hpp>

#include <cstddef>
#include <optional>

namespace rg
{

    template<typename BaseAllocator, size_t Size, unsigned PoolSize = 1024>
    class FreeList
    {
        using Stack
            = boost::lockfree::stack<void*, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<PoolSize>>;
        // using Stack = cdrc::atomic_stack<void*>;
        // can probably be done without extra memory by storing next ptrs in the memory of the void* using reinterpret
        // ala heap layers
        static thread_local inline Stack root_{};

    public:
        FreeList(FreeList const&) = delete;
        FreeList(FreeList&&) = delete;
        FreeList& operator=(FreeList const&) = delete;
        FreeList& operator=(FreeList&&) = delete;

        FreeList() noexcept
        {
        }

        ~FreeList()
        {
            void* curBlock = nullptr;
            // dont really need to pop, only need to iterate
            while(root_.pop(curBlock))
            {
                BaseAllocator::deallocate({curBlock, Size});
            }
        }

        static MemBlk allocate(size_t) noexcept
        {
            void* freeBlock = nullptr;
            if(root_.pop(freeBlock))
            {
                return {freeBlock, Size};
            }
            return BaseAllocator::allocate(Size);
        }

        static void deallocate(MemBlk b) noexcept
        {
            if(root_.push(b.ptr))
            {
                b.reset();
                return;
            }
            return BaseAllocator::deallocate(b);
        }
    };


} // namespace rg
