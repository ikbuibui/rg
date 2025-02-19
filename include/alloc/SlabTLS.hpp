#pragma once

#include "alloc/AffixAllocator.hpp"
#include "alloc/MemBlk.hpp"
#include "math_utils.hpp"

#include <atomic>
#include <cstddef>
#include <memory>

namespace rg
{
    template<typename BaseAllocator, size_t Size, size_t Capacity>
    struct SlabTLSAllocator
    {
        struct BlockMetaData
        {
            std::atomic<std::size_t> dealloc_count{0}; // Active allocations in this block
        };

        using BlockAllocator = PrefixAllocator<BaseAllocator, BlockMetaData>;

        static const constinit size_t alignment = alignof(std::max_align_t);
        static const constinit size_t chunkHeaderSize = std::max(sizeof(BlockMetaData*), alignment);
        static const constinit size_t chunkSize = detail::round_to_alignment(alignment, chunkHeaderSize + Size);
        static const constinit size_t blockSize = chunkSize * Capacity;
        // static const constinit size_t extraSize = BlockAllocator::extraSize + Capacity * chunkHeaderSize... wrong;

    private:
        static MemBlk allocateNewBlock()
        {
            auto memBlk = BlockAllocator::allocate(blockSize);

            // Construct BlockMetaData at the prefix address
            auto& metaData = BlockAllocator::template getPrefix<detail::BlockId::Inner>(memBlk);
            std::construct_at(std::addressof(metaData));

            return memBlk;
        }

        // meant to be used with a singleton to ensure thread safe construction and destruction
        struct CurrentBlock
        {
            CurrentBlock() = default;

            CurrentBlock(CurrentBlock const&) = delete;
            CurrentBlock(CurrentBlock&&) = delete;
            CurrentBlock& operator=(CurrentBlock const&) = delete;
            CurrentBlock& operator=(CurrentBlock&&) = delete;

            // Current active block
            void* userPtr{nullptr};
            // Unused index in the current block
            uint32_t unusedIdx{0};

            ~CurrentBlock()
            {
                // this destructor should be called before BlockAllocator is destroyed. Tough? Need to control the
                // order of destruction
                if(userPtr)
                {
                    void* rawPtr = static_cast<std::byte*>(userPtr) - chunkHeaderSize - unusedIdx * chunkSize;
                    BlockAllocator::deallocate({rawPtr, blockSize});
                }
            }

            void allocateAndSetActiveBlock()
            {
                MemBlk newBlock = allocateNewBlock();
                // Set to the start of the user memory
                userPtr = static_cast<std::byte*>(newBlock.ptr) + chunkHeaderSize;
            }
        };

        static thread_local inline CurrentBlock currentBlock{};

    public:
        // only uses thread locals, no synchronization needed
        static MemBlk allocate(size_t n)
        {
            if(currentBlock.unusedIdx == 0)
            {
                currentBlock.allocateAndSetActiveBlock();
            }
            void* ptr = currentBlock.userPtr;
            ++currentBlock.unusedIdx;
            if(currentBlock.unusedIdx == Capacity)
            {
                currentBlock.unusedIdx = 0;
                currentBlock.userPtr = nullptr;
            }
            else
            {
                currentBlock.userPtr = static_cast<std::byte*>(ptr) + chunkSize;
            }

            return {ptr, n};
        }

        static void deallocate(MemBlk blk)
        {
            auto* headerPtr = reinterpret_cast<BlockMetaData*>(static_cast<std::byte*>(blk.ptr) - chunkHeaderSize);

            // increment dealloc_count, if it hits capacity, call deallocate on the block
            // since only one thread can possibly see this as true, it maybe possible to relax memory order
            if(headerPtr->dealloc_count.fetch_add(1, std::memory_order_acq_rel) == Capacity - 1)
            {
                void* rawPtr = reinterpret_cast<std::byte*>(headerPtr) + BlockAllocator::prefixSize;
                BlockAllocator::deallocate({rawPtr, blockSize});
            }
        }
    };
} // namespace rg
