#include <rg.hpp>

#include <chrono>
#include <cinttypes>
#include <cstdio>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static size_t const iter_count = 1;

inline auto fib(rg::Context ctx, size_t n) -> rg::Task<size_t>
{
    if(n < 2)
    {
        co_return n;
    }

    auto a = co_await rg::dispatch_task<false, true>(fib, ctx, n - 1);
    auto b = co_await rg::dispatch_task<true, true>(fib, ctx, n - 2);
    co_return co_await a.get() + b;
};

auto main_wrapper(rg::Context ctx, size_t n) -> rg::InitTask<int>
{
    co_await rg::dispatch_task(fib, ctx, 30);
    co_await rg::barrier();
    std::printf("results:\n");
    auto startTime = std::chrono::high_resolution_clock::now();

    for(size_t i = 0; i < iter_count; ++i)
    {
        auto result = co_await rg::dispatch_task(fib, ctx, n);
        std::printf("  - %" PRIu64 "\n", co_await result.get());
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto totalTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    std::printf("runs:\n");
    std::printf("  - iteration_count: %" PRIu64 "\n", iter_count);
    std::printf("    duration: %" PRIu64 " us\n", totalTimeUs.count());

    co_return 0;
}

int main(int argc, char* argv[])
{
    if(argc != 2)
    {
        printf("Usage: fib <n-th fibonacci number requested>\n");
        exit(0);
    }
    auto n = static_cast<size_t>(atoi(argv[1]));

    std::printf("threads: %" PRIu64 "\n", thread_count);

    auto poolObj = rg::init(thread_count);
    auto a = main_wrapper(poolObj.getContext(), n);

    return 0;
}
