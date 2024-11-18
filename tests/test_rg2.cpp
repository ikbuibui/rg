// tests/test_rg2.cpp
#define CATCH_CONFIG_MAIN
#include "ThreadPool.hpp"
#include "dispatchTask.hpp"
#include "init.hpp"
#include "initTask.hpp"
#include "resources.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <thread>

// hijack main and return its value from init
// be explicit about type in exposed interface and auto deduce generically inside
template<typename T>
auto orchestrate(rg::ThreadPool* ptr) -> rg::InitTask<T>
{
    std::cout << "init coro started running" << std::endl;

    auto a = co_await rg::dispatch_task(
        []() -> rg::Task<T>
        {
            std::cout << "dispatched task starts executing" << std::endl;

            auto b = co_await rg::dispatch_task(
                []() -> rg::Task<T>
                {
                    std::cout << "child 1 starts executing" << std::endl;
                    co_return 1;
                });

            co_return 1;
        });
    auto out = co_await a.get();
    std::cout << "get a " << out << std::endl;

    auto c = co_await rg::dispatch_task(
        []() -> rg::Task<T>
        {
            std::cout << "child 2 starts executing" << std::endl;
            auto b = co_await rg::dispatch_task(
                []() -> rg::Task<T>
                {
                    auto b = co_await rg::dispatch_task(
                        []() -> rg::Task<T>
                        {
                            std::cout << "child 2.1 starts executing" << std::endl;
                            auto b = co_await rg::dispatch_task(
                                []() -> rg::Task<T>
                                {
                                    std::cout << "child 2.1.1 starts executing" << std::endl;
                                    co_return 1;
                                });
                            auto out = co_await b.get();
                            std::cout << "get b " << out << std::endl;

                            auto c = co_await rg::dispatch_task(
                                []() -> rg::Task<T>
                                {
                                    std::cout << "child 2.1.2 starts executing" << std::endl;
                                    co_return 1;
                                });

                            co_return 1;
                        });

                    std::cout << "child 2.2 starts executing" << std::endl;
                    co_return 1;
                });

            co_return 1;
        });

    // std::this_thread::sleep_for(std::chrono::seconds(4));

    std::cout << "init coro running through" << std::endl;

    co_return 1;
}

// TEST_CASE("Init and Finalize")
// {
//     std::cout << "Position 0" << std::endl;
//     auto poolObj = rg::init(6);
//     std::cout << "Position 1" << std::endl;
//     auto orc = orchestrate<int>(poolObj.pool_ptr());
//     std::cout << "Position 2" << std::endl;
//     auto val = orc.get();
//     std::cout << "Position 3" << std::endl;
//     return;
// }

auto taskWithRes(rg::ThreadPool* ptr) -> rg::InitTask<int>
{
    int aData = 0;
    int bData = 0;
    auto a = rg::makeIOResource<1>(aData);
    auto b = rg::makeIOResource<2>(bData);

    std::cout << "a, a_data: " << a.obj << " " << aData << std::endl;

    auto a_handle = a.rg_read();
    std::cout << "a handle read res id " << a_handle.resource_id << std::endl;
    auto a_handle_w = a.rg_write();
    std::cout << "a handle write res id " << a_handle_w.resource_id << std::endl;
    auto b_handle = b.rg_read();
    std::cout << "b handle res id " << b_handle.resource_id << std::endl;

    co_await rg::dispatch_task(
        [](auto a) -> rg::Task<int>
        {
            std::cout << "Write to A" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            a = 4;
            std::cout << "Write A done" << std::endl;
            co_return 0;
        },
        a.rg_write());
    co_await rg::dispatch_task(
        [](auto a) -> rg::Task<int>
        {
            std::cout << "Read A: " << a << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            co_return 0;
        },
        a.rg_read());

    co_await rg::dispatch_task(
        [](auto b) -> rg::Task<int>
        {
            std::cout << "Write to B" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            b = 7;
            std::cout << "Write B done" << std::endl;
            co_return 0;
        },
        b.rg_write());

    co_await rg::dispatch_task(
        [](auto a, auto b) -> rg::Task<int>
        {
            std::cout << "Read A & B: " << a << ", " << b << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            co_return 0;
        },
        a.rg_read(),
        b.rg_read());
    co_return 0;
}

TEST_CASE("Tasks with resources")
{
    auto poolObj = rg::init(6);
    auto orc = taskWithRes(poolObj.pool_ptr());
    auto val = orc.get();
    return;
}

TEST_CASE("Access type combinations")
{
    rg::ResourceHandle<int*, 1, rg::access::read> readAccess(nullptr);
    rg::ResourceHandle<int*, 2, rg::access::write> writeAccess(nullptr);
    rg::ResourceAccess<3, rg::access::aadd> aaddAccess;
    rg::ResourceAccess<4, rg::access::amul> amulAccess;

    REQUIRE(is_serial_access(readAccess, readAccess) == false); // read & read
    REQUIRE(is_serial_access(aaddAccess, amulAccess) == true); // aadd & amul
    REQUIRE(is_serial_access(writeAccess, readAccess) == true); // write & read
    REQUIRE(is_serial_access(aaddAccess, aaddAccess) == false); // aadd & aadd
    REQUIRE(is_serial_access(amulAccess, amulAccess) == false); // amul & amul
    REQUIRE(is_serial_access(writeAccess, writeAccess) == true); // write & write
}

// TEST_CASE("Hello function works", "[hello]")
// {
//     using namespace std::chrono_literals;
//     int result = call_and_return_coroutine_result(
//         [](int a, int b)
//         {
//             std::this_thread::sleep_for(2000ms);
//             return a + b;
//         },
//         2,
//         3);
//     std::cout << "Result: " << result << std::endl; // Should print 5

//     call_and_return_coroutine_result([](int a, int b) { std::cout << std::endl << a << " " << b << std::endl; }, 2,
//     3);

//     return;

//     // auto rg2_return_obj = rg2::mainCoro([]{
//     //     auto b_awaitable = co_await rg2::emplace_task([]{auto a = 2; auto b = 2*a ; return b;})
//     // });
// }
