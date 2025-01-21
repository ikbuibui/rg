#pragma once

#include "waitCounter.hpp"

#include <boost/lockfree/stack.hpp>

#include <cstddef>

namespace rg
{

    // Special purpose allocators for coroutine frames
    // Inspired by Andrei Alexandrescu's talk on allocators and heap layers
    struct MemBlk
    {
        constexpr MemBlk() noexcept : ptr(nullptr), n(0)
        {
        }

        constexpr MemBlk(void* ptr, size_t n) noexcept : ptr(ptr), n(n)
        {
        }

        constexpr MemBlk(MemBlk&& x) noexcept : ptr{x.ptr}, n{x.n}
        {
            x.reset();
        }

        constexpr MemBlk& operator=(MemBlk&& x) noexcept
        {
            ptr = x.ptr;
            n = x.n;
            x.reset();
            return *this;
        }

        constexpr MemBlk& operator=(MemBlk const& x) noexcept = default;
        constexpr MemBlk(MemBlk const& x) noexcept = default;
        ~MemBlk() = default;

        friend constexpr bool operator==(MemBlk const& lhs, MemBlk const& rhs)
        {
            return lhs.ptr == rhs.ptr && lhs.n == rhs.n;
        }

        constexpr void reset() noexcept
        {
            ptr = nullptr;
            n = 0;
        }

        explicit operator bool() const
        {
            return n != 0;
        }

        void* ptr;
        size_t n;
    };

    template<typename BaseAllocator>
    class AlignedAllocator
    {
    public:
        // Align size to prevent false sharing
        static constexpr std::size_t align_size(std::size_t n)
        {
            // TODO is is better to & with -hardware_destructive_interference_size;
            return (n + hardware_destructive_interference_size - 1u) & ~(hardware_destructive_interference_size - 1u);
        }

        // Allocate memory using the base allocator
        static MemBlk allocate(std::size_t n)
        {
            std::size_t aligned_size = align_size(n);
            return BaseAllocator::allocate(aligned_size);
        }

        // Deallocate memory using the base allocator
        static void deallocate(MemBlk blk)
        {
            BaseAllocator::deallocate(blk);
        }
    };

    class OpNewAllocator
    {
    public:
        static MemBlk allocate(std::size_t n)
        {
            return MemBlk(::operator new(n), n);
        }

        static void deallocate(MemBlk blk)
        {
            ::operator delete(blk.ptr, blk.n);
        }
    };

    template<class BaseAllocator, size_t MinSize, size_t MaxSize, unsigned PoolSize = 1024>
    class FreeList
    {
        using Stack
            = boost::lockfree::stack<void*, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<PoolSize>>;

        static constexpr size_t blockSize = MaxSize;

        // can probably be done without extra memory by storing next ptrs in the memory of the void* using reinterpret
        // ala heap layers
        static inline Stack root_;

    public:
        FreeList(FreeList const&) = delete;
        FreeList(FreeList&&) = delete;
        FreeList& operator=(FreeList const&) = delete;
        FreeList& operator=(FreeList&&) = delete;

        FreeList() noexcept
        {
            root_ = Stack();
        }

        /**
         * Frees all resources. Beware of using allocated blocks given by
         * this allocator after calling this.
         */
        ~FreeList()
        {
            void* curBlock = nullptr;
            // dont really need to pop, only need to iterate
            while(root_.pop(curBlock))
            {
                MemBlk oldBlock(curBlock, MaxSize);
                BaseAllocator::deallocate(oldBlock);
            }
        }

        /**
         * Provides a block. If it is available in the pool, then this will be
         * reused. If the pool is empty, then a new block will be created and
         * returned. The passed size n must be within the boundary of the
         * allocator, otherwise an empty block will returned.
         * Depending on the parameter NumberOfBatchAllocations not only one new
         * block is allocated, but as many as specified.
         * \param n The number of requested bytes. The result is aligned to the
         *          upper boundary.
         * \return The allocated block
         */
        static MemBlk allocate(size_t) noexcept
        {
            MemBlk result;

            void* freeBlock = nullptr;

            if(root_.pop(freeBlock))
            {
                result.ptr = freeBlock;
                result.n = MaxSize;
                return result;
            }

            result = BaseAllocator::allocate(blockSize);

            return result;
        }

        static void deallocate(MemBlk b) noexcept
        {
            if(root_.push(b.ptr))
            {
                b.reset();
                return;
            }
            BaseAllocator::deallocate(b);
        }
    };

} // namespace rg
