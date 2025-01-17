#pragma once

#include "waitCounter.hpp"

#include <cstddef>

namespace rg
{
    struct MemBlk
    {
        constexpr MemBlk() noexcept : ptr(nullptr), n(0)
        {
        }

        constexpr MemBlk(void* ptr, size_t n) noexcept : ptr(ptr), n(n)
        {
        }

        MemBlk(MemBlk&& x) noexcept : ptr{x.ptr}, n{x.n}
        {
            x.reset();
        }

        MemBlk& operator=(MemBlk&& x) noexcept
        {
            ptr = x.ptr;
            n = x.n;
            x.reset();
            return *this;
        }

        MemBlk& operator=(MemBlk const& x) noexcept = default;
        MemBlk(MemBlk const& x) noexcept = default;
        ~MemBlk() = default;

        void reset() noexcept
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

    bool operator==(MemBlk const& lhs, MemBlk const& rhs)
    {
        return lhs.ptr == rhs.ptr && lhs.n == rhs.n;
    }

    template<typename BaseAllocator>
    class AlignedAllocator

    {
    public:
        explicit AlignedAllocator() = default;

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
            ::operator delete(blk.ptr);
        }
    };

} // namespace rg
