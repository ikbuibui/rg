#include <rg.hpp>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <ranges>

static size_t thread_count = std::thread::hardware_concurrency() / 2;
static size_t const iter_count = 1;

inline constexpr int nqueens_work = 14;

inline constexpr std::array<int, 28> answers = {
    0, 1, 0, 0, 2, 10, 4, 40, 92, 352, 724, 2680, 14200, 73712, 365596, 2'279'184, 14'772'512, 95'815'104, 666'090'624,
};

void check_answer(int result)
{
    if(result != answers[nqueens_work])
    {
        std::printf("error: expected %d, got %d\n", answers[nqueens_work], result);
    }
}

template<size_t N>
auto nqueens(int xMax, std::array<char, N> buf) -> rg::Task<int>
{
    if(N == xMax)
    {
        co_return 1;
    }

    int taskCount = 0;
    auto tasks = std::ranges::views::iota(0UL, N)
                 | std::ranges::views::filter(
                     [xMax, &buf](int y)
                     {
                         buf[xMax] = y;
                         char q = y;
                         for(int x = 0; x < xMax; x++)
                         {
                             char p = buf[x];
                             if(q == p || q == p - (xMax - x) || q == p + (xMax - x))
                             {
                                 return false;
                             }
                         }
                         return true;
                     });

    // Spawn up to N tasks (but possibly less, if queens_ok fails)

    std::array<rg::Task<int>, N> parts;
    for([[maybe_unused]] auto t : tasks)
    {
        parts[taskCount] = co_await rg::dispatch_task(nqueens<N>, xMax + 1, buf);
        ++taskCount;
    }

    int ret = 0;
    for(int i = 0; i < taskCount; ++i)
    {
        ret += co_await parts[i].get();
    }

    co_return ret;
};

auto main_wrapper(rg::ThreadPool* ptr) -> rg::InitTask<int>
{
    {
        std::array<char, nqueens_work> buf{};
        auto result = co_await dispatch_task(nqueens<nqueens_work>, 0, buf); // warmup
        check_answer(co_await result.get());
        rg::BarrierAwaiter{};
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    for(size_t i = 0; i < iter_count; ++i)
    {
        std::array<char, nqueens_work> buf{};
        auto result = co_await dispatch_task(nqueens<nqueens_work>, 0, buf);
        auto res = co_await result.get();
        check_answer(res);
        std::printf("  - %d\n", res);
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
    std::printf("threads: %" PRIu64 "\n", thread_count);

    auto poolObj = rg::init(thread_count);
    auto a = main_wrapper(poolObj.pool_ptr());

    return 0;
}
