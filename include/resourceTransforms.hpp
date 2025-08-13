#pragma once
#include <utility>

namespace rg
{
    auto default_transformer = [](auto&& arg) { return std::forward<decltype(arg)>(arg); };
    auto get_transformer = [](auto&& arg) { return std::forward<decltype(arg)>(arg).get(); };
} // namespace rg
