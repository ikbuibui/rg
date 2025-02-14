#pragma once

#include "alloc/AffixAllocator.hpp"
#include "alloc/MemBlk.hpp"

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <new>
#include <type_traits>

namespace rg
{
    template<typename BaseAllocator, size_t Size, size_t Capacity>
    struct ThreadSafeStackAllocator
    {
    private:
        struct BlockMetaData;

        using BlockAllocator = PrefixAllocator<BaseAllocator, BlockMetaData>;

        // Header prepended to each allocation
        struct AllocationHeader
        {
            // Pointer to the block this object belongs to
            BlockMetaData* parent_block;
        };

        struct BlockMetaData
        {
            std::atomic<std::size_t> dealloc_count{0}; // Active allocations in this block

            BlockMetaData() : dealloc_count(0)
            {
            }

            // increment usage count, if usage_count hits capacity, call dellocate on the block
            ~BlockMetaData()
            {
                if(dealloc_count.fetch_add(1, std::memory_order_acq_rel) == Capacity - 1)
                {
                    BlockAllocator::deallocate(
                        {static_cast<void*>(static_cast<std::byte*>(this) - sizeof(BlockMetaData)),
                         (sizeof(AllocationHeader) + Size) * Capacity});
                }
            }
        };

        static inline std::atomic<BlockMetaData*> active_block{nullptr}; // Current active block
        static inline std::mutex block_mutex; // Mutex for switching/adding new blocks

        static BlockMetaData* allocate_new_block()
        {
            std::size_t total_size = (sizeof(AllocationHeader) + Size) * Capacity;
            auto memBlk = BlockAllocator::allocate(total_size);

            BlockMetaData* metadata = BlockAllocator::getPrefix<detail::BlockId::Inner>(memBlk);
            *metadata = BlockMetaData{};
            return metadata;
        }

    public:
        ThreadSafeStackAllocator()
        {
            // TODO meyers singleton for thread safe init
            BlockMetaData* initial_block = allocate_new_block();
            active_block.store(initial_block, std::memory_order_release);
        }

        ~ThreadSafeStackAllocator()
        {
            BlockMetaData* block = active_block.load(std::memory_order_acquire);
            if(block)
            {
                BlockAllocator::deallocate({block->raw_memory, (sizeof(AllocationHeader) + sizeof(T)) * Capacity});
                delete block;
            }
        }

        static MemBlk allocate(size_t)
        {
            BlockMetaData* block = active_block.load(std::memory_order_acquire);

            std::size_t index = block->usage_count.fetch_add(1, std::memory_order_acq_rel);
            if(index < Capacity)
            {
                auto raw_ptr
                    = reinterpret_cast<std::byte*>(block->data) + index * (sizeof(AllocationHeader) + sizeof(T));
                auto header = reinterpret_cast<AllocationHeader*>(raw_ptr);
                header->parent_block = block;

                return reinterpret_cast<T*>(raw_ptr + sizeof(AllocationHeader));
            }

            block->usage_count.fetch_sub(1, std::memory_order_acq_rel); // Rollback the increment
            std::lock_guard<std::mutex> lock(block_mutex);

            block = active_block.load(std::memory_order_acquire);
            if(block->usage_count.load(std::memory_order_acquire) >= Capacity)
            {
                BlockMetaData* new_block = allocate_new_block();
                active_block.store(new_block, std::memory_order_release);
                new_block->usage_count.fetch_add(1, std::memory_order_acq_rel);

                auto raw_ptr = reinterpret_cast<std::byte*>(new_block->data);
                auto header = reinterpret_cast<AllocationHeader*>(raw_ptr);
                header->parent_block = new_block;

                return reinterpret_cast<T*>(raw_ptr + sizeof(AllocationHeader));
            }

            // Another thread may have allocated in the meantime
            block->usage_count.fetch_add(1, std::memory_order_acq_rel);
            auto raw_ptr
                = reinterpret_cast<std::byte*>(block->data)
                  + (block->usage_count.load(std::memory_order_acquire) - 1) * (sizeof(AllocationHeader) + sizeof(T));
            auto header = reinterpret_cast<AllocationHeader*>(raw_ptr);
            header->parent_block = block;

            return reinterpret_cast<T*>(raw_ptr + sizeof(AllocationHeader));
        }

        static void deallocate(MemBlk b)
        {
            auto header_ptr
                = reinterpret_cast<AllocationHeader*>(reinterpret_cast<std::byte*>(b.ptr) - sizeof(AllocationHeader));
            BlockMetaData* block = header_ptr->parent_block;

            if(block->usage_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                if(block != active_block.load(std::memory_order_acquire))
                {
                    std::lock_guard<std::mutex> lock(block_mutex);
                    BaseAllocator::deallocate({block->raw_memory, (sizeof(AllocationHeader) + sizeof(T)) * Capacity});
                    delete block;
                }
            }
        }
    };


} // namespace rg
