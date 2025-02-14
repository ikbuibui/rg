template<class BaseAllocator, size_t BaseMemSize = 512, unsigned NumBlocks = 64>
class SlabAllocator
{
    static inline void* memory_ = nullptr;
    static inline std::atomic<unsigned> currentBlock = 0;
    static inline std::atomic<unsigned> activeCounter = NumBlocks;

public:
    static MemBlk allocate(size_t) noexcept
    {
        if(!memory_)
        {
            memory_ = BaseAllocator::allocate(BaseMemSize * NumBlocks);
        }
        currentBlock.fetch_add(1, std::memory_order_relaxed);
        return MemBlk{memory_, BaseMemSize};
    }

    static void deallocate(MemBlk b) noexcept
    {
    }
};

// Allocates a fixed size block irresoective of size requested.
// User needs to make sure Size > n resquested
template<typename BaseAllocator, std::size_t Size>
struct BlockAllocator
{
    static MemBlk allocate(std::size_t)
    {
        return BaseAllocator::allocate(Size);
    }

    static void deallocate(MemBlk blk)
    {
        BaseAllocator::deallocate({blk.ptr, Size});
    }
};
