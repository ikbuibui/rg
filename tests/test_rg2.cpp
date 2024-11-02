// tests/test_rg2.cpp
#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include "rg2.hpp"

TEST_CASE("Hello function works", "[hello]") {
    int result = call_and_return_coroutine_result([](int a, int b){return a+b;}, 2, 3);
    std::cout << "Result: " << result << std::endl;  // Should print 5
    return;

    // auto rg2_return_obj = rg2::mainCoro([]{
    //     auto b_awaitable = co_await rg2::emplace_task([]{auto a = 2; auto b = 2*a ; return b;})
    // });
}
