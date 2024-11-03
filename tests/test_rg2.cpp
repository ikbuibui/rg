// tests/test_rg2.cpp
#define CATCH_CONFIG_MAIN
#include "finalizeTask.hpp"
#include "initTask.hpp"
#include "resources.hpp"
#include "rg2.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <thread>

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
    auto init = rg::init(6);
    std::cout << "Position A" << std::endl;
    rg::finalize(init.get());
    std::cout << "Position B" << std::endl;
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
