#pragma once

#include "alloc/MemBlk.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <new>
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

        // template<typename Affix, typename Enabled = void>
        // struct affix_creator;

        // template<typename Affix>
        // struct affix_creator<Affix, typename std::enable_if<std::is_default_constructible<Affix>::value>::type>
        // {
        //     template<typename Allocator>
        //     static constexpr void create(void* p, Allocator&)
        //     {
        //         new(p) Affix{};
        //     }
        // };

        // template<typename Affix>
        // struct affix_creator<Affix, typename std::enable_if<!std::is_default_constructible<Affix>::value>::type>
        // {
        //     template<typename Allocator>
        //     static constexpr void create(void* p, Allocator& a)
        //     {
        //         new(p) Affix(a);
        //     }
        // };

        // template<typename Affix, typename Allocator>
        // void create_affix_in_place(void* p, Allocator& a)
        // {
        //     affix_creator<Affix>::create(p, a);
        // }

        struct no_affix
        {
            using value_type = int;
            static constexpr int pattern = 0;
        };
    } // namespace detail

    // getPrefix and getSuffix do not default initialize the affixes, it is the users responsibilty to do it. This to
    // not require default constructible affixes
    template<typename BaseAllocator, typename Prefix = detail::no_affix, typename Suffix = detail::no_affix>
    requires std::is_trivially_destructible_v<Prefix> && std::is_trivially_destructible_v<Suffix>
    struct AffixAllocator
    {
        static const constinit bool has_prefix = !std::is_same<Prefix, detail::no_affix>::value;
        static const constinit bool has_suffix = !std::is_same<Suffix, detail::no_affix>::value;
        static const constinit size_t prefixSize = std::max(sizeof(Prefix), alignof(std::max_align_t));
        static const constinit size_t suffixSize = std::max(sizeof(Suffix), alignof(std::max_align_t));

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

            auto outer_memBlk = BaseAllocator::allocate(block_size);
            return getBlock<detail::BlockId::Inner>(outer_memBlk);

            // void* user_ptr = static_cast<std::byte*>(base_mem.ptr);

            // if constexpr(has_prefix)
            // {
            //     user_ptr = static_cast<std::byte*>(user_ptr) + prefixSize;
            //     detail::create_affix_in_place<Prefix>(getPrefix, BaseAllocator{});
            // }
            // if constexpr(has_suffix)
            // {
            //     void* suffix_ptr = static_cast<std::byte*>(user_ptr) + n;
            //     detail::create_affix_in_place<Suffix>(suffix_ptr, BaseAllocator{});
            // }

            // return {user_ptr, n};
        }

        static void deallocate(MemBlk blk)
        {
            // if constexpr(has_prefix)
            // {
            //     getPrefix<detail::BlockId::Inner>(blk)->~Prefix();
            // }

            // if constexpr(has_suffix)
            // {
            //     getSuffix<detail::BlockId::Inner>(blk)->~Suffix();
            // }

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
