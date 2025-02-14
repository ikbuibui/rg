#pragma once

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

} // namespace rg
