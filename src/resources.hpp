#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

namespace rg
{
    namespace access
    {
        struct read
        {
            using access_type = read;
        };

        struct write
        {
            using access_type = write;
        };

        struct aadd
        {
            using access_type = aadd;
        };

        struct amul
        {
            using access_type = amul;
        };

    } // namespace access

    template<typename T, uint32_t ResourceID, typename AccessMode>
    struct Combined
    {
        using access_type = AccessMode;
        T ptr;

        Combined(T pointer) : ptr(pointer)
        {
        }

        uint32_t getResourceID() const
        {
            return ResourceID;
        }
    };

    template<typename T>
    concept HasAccessType = requires { typename T::access_type; };

    // TODO think one for IOResources and another for others?
    template<HasAccessType A, HasAccessType B>
    bool is_serial_access(A const&, B const&)
    {
        using AccessA = typename A::access_type;
        using AccessB = typename B::access_type;

        return !static_cast<bool>(
            (std::is_same_v<AccessA, access::read> && std::is_same_v<AccessB, access::read>)
            || (std::is_same_v<AccessA, access::aadd> && std::is_same_v<AccessB, access::aadd>)
            || (std::is_same_v<AccessA, access::amul> && std::is_same_v<AccessB, access::amul>) );
    }

    // Function to bind Combined value to a callable
    template<typename T, typename AccessMode, uint32_t ResourceID, typename Func>
    auto bindToCallable(Combined<T, ResourceID, AccessMode> const& combined, Func&& f)
    {
        // Use std::bind to create a callable with the value from combined.ptr
        return std::bind(std::forward<Func>(f), *combined.ptr);
    }

    template<typename T, uint32_t ResourceID>
    class IOResource : public T
    {
    public:
        using T::T;

        Combined<T const*, ResourceID, access::read> rg_read()
        {
            return {static_cast<T const*>(this)};
        }

        Combined<T*, ResourceID, access::write> rg_write()
        {
            return {static_cast<T*>(this)};
        }
    };

    template<typename T>
    auto createIOResource()
    {
        // TODO use a better counter
        return IOResource<T, __COUNTER__>{};
    }
} // namespace rg
