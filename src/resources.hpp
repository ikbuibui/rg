#pragma once

#include <concepts>
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
    struct ResourceHandle
    {
        using access_type = typename AccessMode::access_type;
        static constexpr uint32_t resource_id = ResourceID;

        // handle
        T obj;

        ResourceHandle(T&& t) : obj(std::forward<T>(t))
        {
        }
    };

    template<uint32_t ResourceID, typename AccessMode>
    struct ResourceAccess
    {
        static constexpr uint32_t resource_id = ResourceID;
        using access_type = AccessMode::access_type;
    };

    // Concept to ensure a type is ResourceAccess
    template<typename T>
    concept IsResourceHandle = requires {
        // Must have a static constexpr variable `resource_id`
        T::resource_id;
        // Must have a nested `access_type` type
        typename T::access_type;
        // must have obj which is the type which is bound to the callable
        T::obj;
    };

    template<typename... Ts>
    struct TypeList
    {
        template<typename Func>
        static constexpr void for_each(Func&& func)
        {
            (func.template operator()<Ts>(), ...); // Fold expression
        }
    };

    template<typename T>
    struct ExtractResourceID;

    template<typename T, uint32_t ResourceID, typename AccessMode>
    struct ExtractResourceID<ResourceHandle<T, ResourceID, AccessMode>>
    {
        using id = std::integral_constant<uint32_t, ResourceHandle<T, ResourceID, AccessMode>::resource_id>;
    };

    template<typename Tuple, std::size_t... Is>
    auto extractResourceIDsImpl(std::index_sequence<Is...>)
    {
        return TypeList<typename ExtractResourceID<std::tuple_element_t<Is, Tuple>>::id...>{};
    }

    template<typename Callable, typename... Args>
    struct ResourceIDExtractor
    {
        using TupleType = std::tuple<Args...>;

        using type = decltype(extractResourceIDsImpl<TupleType>(std::make_index_sequence<sizeof...(Args)>{}));
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
    auto bindToCallable(ResourceHandle<T, ResourceID, AccessMode> const& combined, Func&& f)
    {
        // Use std::bind to create a callable with the value from combined.ptr
        return std::bind(std::forward<Func>(f), *combined.ptr);
    }

    // template<typename T, uint32_t ResourceID>
    // class IOResource : public T
    // {
    // public:
    //     using T::T;

    // TODO overload on const

    //     ResourceHandle<T const&, ResourceID, access::read> rg_read()
    //     {
    //         return {static_cast<T const>(this)};
    //     }

    //     ResourceHandle<T&, ResourceID, access::write> rg_write()
    //     {
    //         return {static_cast<T>(this)};
    //     }
    // };

    template<typename T, uint32_t ResourceID>
    class IOResource
    {
    public:
        T obj;

        // Constructor for both value and reference types
        template<typename U>
        requires std::is_constructible_v<T, U&&>
        explicit IOResource(U&& value) : obj(std::forward<U>(value))
        {
        }

        ResourceHandle<T const, ResourceID, access::read> rg_read()
        {
            return {static_cast<T const>(obj)};
        }

        ResourceHandle<T, ResourceID, access::write> rg_write()
        {
            return {static_cast<T>(obj)};
        }
    };

    // constructs from T, maybe  construct from args. liek empalce back
    template<uint32_t ResourceID, typename T>
    auto makeIOResource(T&& t)
    {
        // TODO use a better counter
        return IOResource<T, ResourceID>{std::forward<T>(t)};
    }
} // namespace rg
