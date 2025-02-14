#pragma once

#include "alloc/AffixAllocator.hpp"
#include "alloc/MemBlk.hpp"
#include "alloc/Singleton.hpp"
#include "math_utils.hpp"

#include <atomic>
#include <cstddef>
#include <memory>

namespace rg
{
    template<typename BaseAllocator, size_t Size, size_t Capacity>
    struct SlabTLSAllocator
    {
    private:
        struct BlockMetaData
        {
            std::atomic<std::size_t> dealloc_count{0}; // Active allocations in this block
        };

        using BlockAllocator = PrefixAllocator<BaseAllocator, BlockMetaData>;

        static const constinit size_t chunkHeaderSize = std::max(sizeof(BlockMetaData*), alignof(std::max_align_t));

        // variables defining the size of the memory with the allocation header
        static const constinit size_t chunkSize = chunkHeaderSize + Size;
        static const constinit size_t blockSize = chunkSize * Capacity;

        static BlockMetaData* allocateNewBlock()
        {
            auto memBlk = BlockAllocator::allocate(blockSize);
            BlockMetaData* metadata = std::construct_at(
                std::addressof(BlockAllocator::template getPrefix<detail::BlockId::Inner>(memBlk)));
            return metadata;
        }

        // meant to be used with a singleton to ensure thread safe construction and destruction
        struct CurrentBlock
        {
            // TODO should i delete it
            CurrentBlock() = default;

            CurrentBlock(CurrentBlock const&) = delete;
            CurrentBlock(CurrentBlock&&) = delete;
            CurrentBlock& operator=(CurrentBlock const&) = delete;
            CurrentBlock& operator=(CurrentBlock&&) = delete;
            // Current active block
            static thread_local inline void* userPtr{nullptr};

            ~CurrentBlock()
            {
                // this destructor should be called before BlockAllocator is destroyed. Tough? Need to control the
                // order of destruction
                if(userPtr)
                {
                    void* rawPtr = static_cast<std::byte*>(userPtr) - unusedIdx * chunkSize;
                    BlockAllocator::deallocate({rawPtr, blockSize});
                }
            }

            void allocateAndSetActiveBlock()
            {
                BlockMetaData* newBlock = allocateNewBlock();
                // Set to the start of the user memory
                userPtr = static_cast<std::byte*>(newBlock) + BlockAllocator::prefixSize + chunkHeaderSize;
            }
        };

        using SingletonCurrentBlock = Singleton<CurrentBlock>;

        // Unused index in the current block
        static inline thread_local uint32_t unusedIdx{0};

    public:
        static MemBlk allocate(size_t)
        {
            if(unusedIdx == 0)
            {
                SingletonCurrentBlock::getInstance().allocateAndSetActiveBlock();
            }

            auto ptr = static_cast<std::byte*>(SingletonCurrentBlock::getInstance().userPtr);

            SingletonCurrentBlock::getInstance().userPtr
                = static_cast<std::byte*>(SingletonCurrentBlock::getInstance().userPtr) + chunkSize;
            ++unusedIdx;
            if(unusedIdx == Capacity)
            {
                unusedIdx = 0;
                SingletonCurrentBlock::getInstance().userPtr = nullptr;
            }

            return {ptr, Size};
        }

        static void deallocate(MemBlk blk)
        {
            auto* headerPtr = reinterpret_cast<BlockMetaData*>(static_cast<std::byte*>(blk.ptr) - chunkHeaderSize);

            // increment dealloc_count, if it hits capacity, call deallocate on the block
            // since only one thread can possibly see this as true, it maybe possible to relax memory order
            if(headerPtr->dealloc_count.fetch_add(1, std::memory_order_acq_rel) == Capacity - 1)
            {
                void* rawPtr = static_cast<std::byte*>(headerPtr) + BlockAllocator::prefixSize;
                BlockAllocator::deallocate({rawPtr, blockSize});
            }
        }
    };


} // namespace rg
