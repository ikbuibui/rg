
#pragma once

#include "alloc/AffixAllocator.hpp"
#include "alloc/MemBlk.hpp"
#include "alloc/Singleton.hpp"
#include "cdrc_stack.hpp"

#include <boost/lockfree/stack.hpp>

#include <cstddef>
#include <optional>

namespace rg
{

    template<typename BaseAllocator, size_t Size, unsigned PoolSize = 1024>
    class FreeListTLS
    {
        using StackType
            = boost::lockfree::stack<void*, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<PoolSize>>;

        // using Stack = cdrc::atomic_stack<void*>;
        // can probably be done without extra memory by storing next ptrs in the memory of the void* using reinterpret
        // ala heap layers

        struct Stack
        {
            static thread_local inline StackType root_{};

            ~Stack()
            {
                root_.consume_all([](void* curPtr) { TaggedAlloc::deallocate({curPtr, Size}); });
            }
        };

        using SingletonStack = Singleton<Stack>;

        using TaggedAlloc = PrefixAllocator<BaseAllocator, SingletonStack*>;

    public:
        // pop from thread local free list, if empty, allocate from tagged allocator
        static MemBlk allocate(size_t) noexcept
        {
            void* freeBlock = nullptr;
            if(SingletonStack::getInstance().root_.pop(freeBlock))
            {
                return {freeBlock, Size};
            }
            auto innerBlk = TaggedAlloc::allocate(Size);
            TaggedAlloc::template getPrefix<detail::BlockId::Inner>(innerBlk) = &SingletonStack::getInstance();
            return innerBlk;
        }

        // push to the free list of allocating thread using the prefix stored
        static void deallocate(MemBlk blk) noexcept
        {
            auto& prefix = TaggedAlloc::template getPrefix<detail::BlockId::Inner>(blk);

            if(prefix->getInstance().root_.push(blk.ptr))
            {
                blk.reset();
                return;
            }
            // maybe try to add to own cache?
            return TaggedAlloc::deallocate(blk);
        }
    };


} // namespace rg
