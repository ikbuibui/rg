#pragma once

#include "alloc/MemBlk.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace rg
{
    namespace detail
    {
        enum class BlockId
        {
            Inner,
            Outer
        };

        struct no_affix
        {
            using value_type = int;
            static constexpr int pattern = 0;
        };

        constexpr size_t round_to_alignment(size_t alignment, size_t size)
        {
            return (size + alignment - 1) & ~(alignment - 1);
        }
    } // namespace detail

    template<typename BaseAllocator, typename Prefix = detail::no_affix, typename Suffix = detail::no_affix>
    requires std::is_trivially_destructible_v<Prefix> && std::is_trivially_destructible_v<Suffix>
    struct AffixAllocator
    {
        static const constinit bool has_prefix = !std::is_same<Prefix, detail::no_affix>::value;
        static const constinit bool has_suffix = !std::is_same<Suffix, detail::no_affix>::value;
        static const constinit size_t prefixSize
            = detail::round_to_alignment(alignof(std::max_align_t), sizeof(Prefix));
        static const constinit size_t suffixSize
            = detail::round_to_alignment(alignof(std::max_align_t), sizeof(Suffix));
        static const constinit size_t extraSize = prefixSize + suffixSize;
        static const constinit size_t maxAlignment
            = std::max({alignof(Prefix), alignof(Suffix), alignof(std::max_align_t)});

        // Get the Prefix object from the full affixed block
        template<detail::BlockId ID>
        static constexpr Prefix& getPrefix(MemBlk blk) noexcept
        {
            if constexpr(ID == detail::BlockId::Inner)
            {
                return *reinterpret_cast<Prefix*>(static_cast<std::byte*>(blk.ptr) - prefixSize);
            }
            if constexpr(ID == detail::BlockId::Outer)
            {
                return *reinterpret_cast<Prefix*>(blk.ptr);
            }
        }

        // Get the Suffix object from the full affixed block
        template<detail::BlockId ID>
        static constexpr Suffix& getSuffix(MemBlk blk) noexcept
        {
            if constexpr(ID == detail::BlockId::Inner)
            {
                return *reinterpret_cast<Suffix*>(static_cast<std::byte*>(blk.ptr) + blk.n);
            }
            if constexpr(ID == detail::BlockId::Outer)
            {
                return *reinterpret_cast<Suffix*>(static_cast<std::byte*>(blk.ptr) + blk.n - suffixSize);
            }
        }

        // Get the ID block from the other block
        template<detail::BlockId ID>
        static constexpr MemBlk getBlock(MemBlk blk) noexcept
        {
            if constexpr(ID == detail::BlockId::Inner)
            {
                void* ptr = blk.ptr;
                size_t n = blk.n;

                if constexpr(has_prefix)
                {
                    ptr = static_cast<std::byte*>(ptr) + prefixSize;
                    n -= prefixSize;
                }
                if constexpr(has_suffix)
                {
                    n -= suffixSize;
                }
                return {ptr, n};
            }
            if constexpr(ID == detail::BlockId::Outer)
            {
                void* ptr = blk.ptr;
                size_t n = blk.n;

                if constexpr(has_prefix)
                {
                    ptr = static_cast<std::byte*>(ptr) - prefixSize;
                    n += prefixSize;
                }
                if constexpr(has_suffix)
                {
                    n += suffixSize;
                }
                n = detail::round_to_alignment(maxAlignment, n);
                return {ptr, n};
            }
        }

        // Returns a ptr to memory of size n. This memory is surrounded with the affix
        static MemBlk allocate(std::size_t n)
        {
            size_t block_size = n;

            if constexpr(has_prefix)
            {
                block_size += prefixSize;
            }
            if constexpr(has_suffix)
            {
                block_size += suffixSize;
            }

            block_size = detail::round_to_alignment(maxAlignment, block_size);

            auto outer_memBlk = BaseAllocator::allocate(block_size);
            return getBlock<detail::BlockId::Inner>(outer_memBlk);
        }

        static void deallocate(MemBlk blk)
        {
            return BaseAllocator::deallocate(getBlock<detail::BlockId::Outer>(blk));
        }
    };

    template<typename BaseAllocator, typename Prefix>
    struct PrefixAllocator : public AffixAllocator<BaseAllocator, Prefix, detail::no_affix>
    {
    public:
        using Base = AffixAllocator<BaseAllocator, Prefix, detail::no_affix>;

        // You can add additional functionality specific to PrefixAllocator here
    };

    template<typename BaseAllocator, typename Suffix>
    struct SuffixAllocator : public AffixAllocator<BaseAllocator, detail::no_affix, Suffix>
    {
    public:
        using Base = AffixAllocator<BaseAllocator, detail::no_affix, Suffix>;
    };
} // namespace rg
