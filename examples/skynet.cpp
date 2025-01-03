// The skynet benchmark as described here:
// https://github.com/atemerev/skynet

#include <rg.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <ranges>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static size_t const iter_count = 1;

template<size_t DepthMax>
rg::Task<size_t> skynet_one(size_t BaseNum, size_t Depth)
{
    if(Depth == DepthMax)
    {
        co_return BaseNum;
    }
    size_t depthOffset = 1;
    for(size_t i = 0; i < DepthMax - Depth - 1; ++i)
    {
        depthOffset *= 10;
    }

    std::array<rg::Task<size_t>, 10> children;
    for(size_t idx = 0; idx < 10; ++idx)
    {
        children[idx] = co_await rg::dispatch_task(skynet_one<DepthMax>, BaseNum + depthOffset * idx, Depth + 1);
    }

    size_t count = 0;
    for(size_t idx = 0; idx < 10; ++idx)
    {
        count += co_await children[idx].get();
    }
    co_return count;
}

template<size_t DepthMax>
rg::Task<void> skynet()
{
    auto handle = co_await rg::dispatch_task(skynet_one<DepthMax>, 0, 0);
    size_t count = co_await handle.get();
    if(count != 4'999'999'950'000'000)
    {
        std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
    }
    co_return;
}

template<size_t Depth = 6>
rg::Task<void> loop_skynet()
{
    std::printf("runs:\n");
    auto startTime = std::chrono::high_resolution_clock::now();
    for(size_t j = 0; j < iter_count; ++j)
    {
        co_await rg::dispatch_task(skynet<Depth>);
        co_await rg::BarrierAwaiter{};
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}

auto main_wrapper(rg::ThreadPool* ptr) -> rg::InitTask<int>
{
    co_await rg::dispatch_task(skynet<8>); // warmup
    co_await rg::BarrierAwaiter{};
    co_await rg::dispatch_task(loop_skynet<8>);
    co_return 0;
}

int main()
{
    std::printf("threads: %" PRIu64 "\n", thread_count);

    auto poolObj = rg::init(thread_count);
    auto a = main_wrapper(poolObj.pool_ptr());

    a.finalize();

    return 0;
}
