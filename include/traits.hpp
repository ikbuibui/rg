#pragma once
#include <type_traits>

namespace rg::traits
{
    template<typename, template<typename...> typename>
    struct is_specialization_of : std::false_type
    {
    };

    template<template<typename...> typename Template, typename... Args>
    struct is_specialization_of<Template<Args...>, Template> : std::true_type
    {
    };

    template<typename T, template<typename...> typename Template>
    inline constexpr bool is_specialization_of_v = is_specialization_of<T, Template>::value;

    template<typename T, template<typename...> typename Template>
    concept IsSpecializationOf = is_specialization_of_v<T, Template>;

} // namespace rg::traits
