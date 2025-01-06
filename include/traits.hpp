#pragma once
#include <type_traits>

namespace rg::traits
{
    // Primary template for is_specialization_of
    template<template<typename...> class, typename>
    struct is_specialization_of : std::false_type
    {
    };

    // Specialization for types that are specializations of the template
    template<template<typename...> class Template, typename... Args>
    struct is_specialization_of<Template, Template<Args...>> : std::true_type
    {
    };

    // Variable template for is_specialization_of
    template<template<typename...> class Template, typename T>
    inline constexpr bool is_specialization_of_v = is_specialization_of<Template, T>::value;


} // namespace rg::traits
