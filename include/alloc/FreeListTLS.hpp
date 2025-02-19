
#pragma once

#include "alloc/AffixAllocator.hpp"
#include "alloc/MemBlk.hpp"

#include <boost/lockfree/stack.hpp>

#include <cstddef>

namespace rg
{

    template<typename BaseAllocator, size_t Size, unsigned PoolSize = 16>
    struct FreeListTLS
    {
        using StackType
            = boost::lockfree::stack<void*, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<PoolSize>>;

        // using StackType = cdrc::atomic_stack<void*>;
        // can probably be done without extra memory by storing next ptrs in the memory of the void* using reinterpret
        // ala heap layers

        struct Stack
        {
            Stack() = default;
            Stack(Stack const&) = delete;
            Stack(Stack&&) = delete;
            Stack& operator=(Stack const&) = delete;
            Stack& operator=(Stack&&) = delete;

            StackType root_{};

            ~Stack()
            {
                root_.consume_all([](void* curPtr) { TaggedAlloc::deallocate({curPtr, Size}); });
            }
        };

        static thread_local inline Stack stack{};

        using TaggedAlloc = PrefixAllocator<BaseAllocator, Stack*>;

        static const constinit size_t extraSize = TaggedAlloc::extraSize;

        // pop from thread local free list, if empty, allocate from tagged allocator
        static MemBlk allocate(size_t n) noexcept
        {
            void* freeBlock = nullptr;
            if(stack.root_.pop(freeBlock))
            {
                return {freeBlock, Size};
            }
            auto innerBlk = TaggedAlloc::allocate(Size);
            TaggedAlloc::template getPrefix<detail::BlockId::Inner>(innerBlk) = &stack;
            return {innerBlk.ptr, n};
        }

        // push to the free list of allocating thread using the prefix stored
        static void deallocate(MemBlk blk) noexcept
        {
            auto& prefix = TaggedAlloc::template getPrefix<detail::BlockId::Inner>(blk);

            if(prefix->root_.push(blk.ptr))
            {
                blk.reset();
                return;
            }
            // maybe try to add to own cache?
            return TaggedAlloc::deallocate({blk.ptr, Size});
        }
    };


} // namespace rg
