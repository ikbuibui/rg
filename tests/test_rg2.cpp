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

TEST_CASE("Init and Finalize", "[hello]")
{
    std::cout << "Position 0" << std::endl;
    auto poolObj = rg::init(6);
    std::cout << "Position 1" << std::endl;
    auto orc = orchestrate<int>(poolObj.pool_ptr());
    std::cout << "Position 2" << std::endl;
    return;
}

TEST_CASE("Access type combinations")
{
    rg::Combined<int*, 1, rg::access::read> readAccess(nullptr);
    rg::Combined<int*, 2, rg::access::write> writeAccess(nullptr);
    rg::Combined<int*, 3, rg::access::aadd> aaddAccess(nullptr);
    rg::Combined<int*, 4, rg::access::amul> amulAccess(nullptr);

    REQUIRE(is_serial_access(readAccess, readAccess) == false); // read & read
    REQUIRE(is_serial_access(aaddAccess, amulAccess) == true); // aadd & amul
    REQUIRE(is_serial_access(writeAccess, readAccess) == true); // write & read
    REQUIRE(is_serial_access(aaddAccess, aaddAccess) == false); // aadd & aadd
    REQUIRE(is_serial_access(amulAccess, amulAccess) == false); // amul & amul
    REQUIRE(is_serial_access(writeAccess, writeAccess) == true); // write & write
}
