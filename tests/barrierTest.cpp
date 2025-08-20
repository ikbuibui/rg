#include "ThreadPool.hpp"

#include <rg.hpp>

#include <chrono>
#include <thread>

// Define a simple data type for the resource.
// This can be any type; int is used here as a placeholder.
struct DummyResourceData
{
    int value;
};

// The core test logic, refactored into an rg::InitTask.
auto barrier_test_logic(rg::Context ctx) -> rg::InitTask<int>
{
    rg::Resource<DummyResourceData> resource1;

    auto handle = co_await rg::dispatch_task(
        ctx.poolPtr,
        [](rg::Context, auto) -> rg::Task<int>
        {
            std::cout << "going to sleep" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::cout << "waking from sleep" << std::endl;
            co_return 1;
        },
        ctx,
        resource1.rg_write());

    std::cout << "before barrier" << std::endl;
    co_await rg::barrier(resource1);
    std::cout << "after barrier" << std::endl;
    std::cout << "output value " << co_await handle.get() << std::endl;
    co_await rg::dispatch_task(
        ctx.poolPtr,
        [](rg::Context, auto res_access) -> rg::Task<void>
        {
            std::cout << "second access to resource" << std::endl;
            (*res_access).value = 42;
            co_return;
        },
        ctx,
        resource1.rg_write());

    co_return 1;
}

int main()
{
    auto poolScope = rg::init(2u);

    auto a = barrier_test_logic(poolScope.getContext());
}
