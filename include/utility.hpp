#pragma once

#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

namespace rg::util
{
    template<typename T>
    void display_type()
    {
        std::string func_name(__PRETTY_FUNCTION__);
        std::string tmp = func_name.substr(func_name.find_first_of("[") + 1);
        std::string type = "type" + tmp.substr(1, tmp.size() - 2);
        std::cout << type << std::endl;
    }

    template<typename T>
    concept CanApplyEBO = !std::is_void_v<T> && std::is_empty_v<T> && !std::is_final_v<T>;


    template<typename T>
    class Compressed;

    template<>
    class Compressed<void>
    {
    public:
        constexpr Compressed() noexcept = default;

        template<typename... Args>
        constexpr explicit Compressed(Args&&...) noexcept
        {
        }

        constexpr void get() const noexcept
        {
        }
    };

    template<typename T>
    requires CanApplyEBO<T>
    class Compressed<T> : private T
    {
        // Inherits from T for EBO

    public:
        constexpr Compressed() = default;

        template<typename... Args>
        constexpr explicit Compressed(Args&&... args) : T(std::forward<Args>(args)...)
        {
        }

        constexpr T& get() noexcept
        {
            return *this;
        }

        constexpr T const& get() const noexcept
        {
            return *this;
        }
    };

    template<typename T>
    class Compressed
    {
    private:
        T value;

    public:
        constexpr Compressed() = default;

        template<typename... Args>
        constexpr explicit Compressed(Args&&... args) : value(std::forward<Args>(args)...)
        {
        }

        constexpr T& get() noexcept
        {
            return value;
        }

        constexpr T const& get() const noexcept
        {
            return value;
        }
    };

} // namespace rg::util
