#pragma once
#include <type_traits>

namespace rg::traits
{
    // Primary template for is_specialization_of
    template<typename, template<typename...> typename>
    struct is_specialization_of : std::false_type
    {
    };

    // Specialization for types that are specializations of the template
    template<template<typename...> typename Template, typename... Args>
    struct is_specialization_of<Template<Args...>, Template> : std::true_type
    {
    };

    // Variable template for is_specialization_of
    template<typename T, template<typename...> typename Template>
    inline constexpr bool is_specialization_of_v = is_specialization_of<T, Template>::value;


} // namespace rg::traits
