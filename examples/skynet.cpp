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
rg::Task<size_t> skynet_one(rg::Context ctx, size_t BaseNum, size_t Depth)
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
        children[idx] = co_await rg::dispatch_task(
            ctx.poolPtr,
            skynet_one<DepthMax>,
            ctx,
            BaseNum + depthOffset * idx,
            Depth + 1);
    }

    size_t count = 0;
    for(size_t idx = 0; idx < 10; ++idx)
    {
        count += co_await children[idx].get();
    }
    co_return count;
}

template<size_t DepthMax>
rg::Task<void> skynet(rg::Context ctx)
{
    auto handle = co_await rg::dispatch_task(ctx.poolPtr, skynet_one<DepthMax>, ctx, 0, 0);
    size_t count = co_await handle.get();
    if(count != 4'999'999'950'000'000)
    {
        std::printf("ERROR: wrong result - %" PRIu64 "\n", count);
    }
    co_return;
}

template<size_t Depth = 6>
rg::Task<void> loop_skynet(rg::Context ctx)
{
    std::printf("runs:\n");
    auto startTime = std::chrono::high_resolution_clock::now();
    for(size_t j = 0; j < iter_count; ++j)
    {
        co_await rg::dispatch_task(ctx.poolPtr, skynet<Depth>, ctx);
        co_await rg::barrier();
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());
}

auto main_wrapper(rg::Context ctx) -> rg::InitTask<int>
{
    co_await rg::dispatch_task(ctx.poolPtr, skynet<8>, ctx); // warmup
    co_await rg::barrier();
    co_await rg::dispatch_task(ctx.poolPtr, loop_skynet<8>, ctx);
    co_return 0;
}

int main()
{
    std::printf("threads: %" PRIu64 "\n", thread_count);

    auto poolObj = rg::init(thread_count);
    auto a = main_wrapper(poolObj.getContext());

    return 0;
}
