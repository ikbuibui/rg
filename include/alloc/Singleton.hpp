#pragma once

#include <utility>

namespace rg
{
    template<typename T>
    class Singleton : public T
    {
    public:
        static Singleton& getInstance()
        {
            static Singleton instance;
            return instance;
        }

    private:
        Singleton(auto&&... args) : T(std::forward<decltype(args)>(args)...)
        {
        }

        ~Singleton() = default;
        Singleton(Singleton const&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator=(Singleton const&) = delete;
        Singleton& operator=(Singleton&&) = delete;
    };


} // namespace rg
