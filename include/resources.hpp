#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace rg
{
    enum class AccessMode : uint8_t
    {
        Read,
        Write,
        AAdd,
        AMul,
    };

    namespace range_access
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

    } // namespace range_access

    // Global ID generator
    class GlobalIDGenerator
    {
    public:
        static uint32_t generate_id()
        {
            return id_counter.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        static std::atomic<uint32_t> id_counter;
    };

    // Initialize the global ID counter
    std::atomic<uint32_t> GlobalIDGenerator::id_counter = 0;

    // template<typename T, uint32_t ResourceID, typename AccessMode>
    // struct ResourceHandle
    // {
    //     using value_type = T&;
    //     using access_type = typename AccessMode::access_type;
    //     static constexpr uint32_t resource_id = ResourceID;

    //     // handle
    //     T& obj;

    //     ResourceHandle(T&& t) : obj(std::forward<T>(t))
    //     {
    //     }
    // };

    // template<uint32_t ResourceID, typename AccessMode>
    // struct ResourceAccess
    // {
    //     static constexpr uint32_t resource_id = ResourceID;
    //     using access_type = AccessMode::access_type;
    // };

    // // Concept to ensure a type is ResourceAccess
    // template<typename T>
    // concept IsResourceHandle = requires {
    //     // Must have a static constexpr variable `resource_id`
    //     T::resource_id;
    //     // Must have a nested `value_type` type
    //     typename T::value_type;
    //     // Must have a nested `access_type` type
    //     typename T::access_type;
    //     // must have obj which is the type which is bound to the callable
    //     T::obj;
    // };

    template<typename... Ts>
    struct TypeList
    {
        template<typename Func>
        static constexpr void for_each(Func&& func)
        {
            (func.template operator()<Ts>(), ...); // Fold expression
        }
    };

    // template<typename T>
    // struct ExtractResourceID;

    // template<typename T, uint32_t ResourceID, typename AccessMode>
    // struct ExtractResourceID<ResourceHandle<T, ResourceID, AccessMode>>
    // {
    //     using id = std::integral_constant<uint32_t, ResourceHandle<T, ResourceID, AccessMode>::resource_id>;
    // };

    // template<typename Tuple, std::size_t... Is>
    // auto extractResourceIDsImpl(std::index_sequence<Is...>)
    // {
    //     return TypeList<typename ExtractResourceID<std::tuple_element_t<Is, Tuple>>::id...>{};
    // }

    // template<typename Callable, typename... Args>
    // struct ResourceIDExtractor
    // {
    //     using TupleType = std::tuple<Args...>;

    //     using type = decltype(extractResourceIDsImpl<TupleType>(std::make_index_sequence<sizeof...(Args)>{}));
    // };

    template<typename T>
    concept HasAccessType = requires { typename T::access_type; };

    template<typename T>
    concept NotAccessType = !HasAccessType<T>;

    // TODO think one for IOResources and another for others?
    bool is_serial_access(AccessMode const a, AccessMode const b)
    {
        return !(
            (a == AccessMode::Read && b == AccessMode::Read) || (a == AccessMode::AAdd && b == AccessMode::AAdd)
            || (a == AccessMode::AMul && b == AccessMode::AMul));
    }

    // Function to bind Combined value to a callable
    // template<typename T, typename AccessMode, uint32_t ResourceID, typename Func>
    // auto bindToCallable(ResourceHandle<T, ResourceID, AccessMode> const& combined, Func&& f)
    // {
    //     // Use std::bind to create a callable with the value from combined.ptr
    //     return std::bind(std::forward<Func>(f), *combined.ptr);
    // }

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

    // template<typename T, uint32_t ResourceID>
    // class IOResource
    // {
    // public:
    //     T& obj;

    //     // Constructor for both value and reference types
    //     template<typename U>
    //     requires std::is_constructible_v<T, U&&>
    //     explicit IOResource(U&& value) : obj(std::forward<U>(value))
    //     {
    //     }

    //     ResourceHandle<T const, ResourceID, access::read> rg_read()
    //     {
    //         return {static_cast<T const>(obj)};
    //     }

    //     ResourceHandle<T, ResourceID, access::write> rg_write()
    //     {
    //         return {static_cast<T>(obj)};
    //     }
    // };


    // template<typename T>
    // class IOResource
    // {
    // private:
    //     uint32_t ResourceID;

    // public:
    //     T obj;

    //     // Constructor for both value and reference types
    //     template<typename U>
    //     requires std::is_constructible_v<T, U&&>
    //     explicit IOResource(U&& value) : obj(std::forward<U>(value))
    //     {
    //     }

    //     ResourceHandle<T const, ResourceID, access::read> rg_read()
    //     {
    //         return {static_cast<T const>(obj)};
    //     }

    //     ResourceHandle<T, ResourceID, access::write> rg_write()
    //     {
    //         return {static_cast<T>(obj)};
    //     }
    // };

    // // constructs from T, maybe  construct from args. liek empalce back
    // template<uint32_t ResourceID, typename T>
    // auto makeIOResource(T&& t)
    // {
    //     // TODO use a better counter
    //     return IOResource<T, ResourceID>{std::forward<T>(t)};
    // }


    // forward declaration to hold shared pointer
    struct ResourceNode;

    template<typename TRes>
    class ResourceAccess
    {
    private:
        std::reference_wrapper<TRes> resource;
        AccessMode accessMode;

    public:
        using access_type = AccessMode;

        // using value_type = T&;

        ResourceAccess(TRes& r, AccessMode access) : resource(r), accessMode(access)
        {
        }

        auto const& get() const
        {
            return resource.get().get();
        }

        auto& get()
        {
            return resource.get().get();
        }

        // read only get?
        // T const& get() const
        // {
        //     return resource.get().get();
        // }

        uint32_t getID() const
        {
            return resource.get().getUserQueue()->getId();
        }

        std::shared_ptr<ResourceNode> const& getUserQueue() const
        {
            return resource.get().getUserQueue();
        }

        AccessMode const& getAccessMode() const
        {
            return accessMode;
        }
    };

    template<typename T>
    class Resource
    {
    private:
        // Use std::variant to manage storage of value or reference
        std::variant<T, std::reference_wrapper<T>> storage;
        std::shared_ptr<ResourceNode> userQueue = std::make_shared<ResourceNode>(GlobalIDGenerator::generate_id());

    public:
        // Constructor for lvalue (stores a reference)
        Resource(T& ref) : storage(std::forward<std::reference_wrapper<T>>(ref))
        {
            // std::cout << "Stored reference to lvalue\n";
        }

        // Constructor for rvalue (temporary value)
        Resource(T&& value) : storage(std::move(value))
        {
            // std::cout << "Moved temporary value into Resource" << std::endl;
        }

        Resource() : storage(std::move(T{}))
        {
            // std::cout << "Default construct T" << std::endl;
        }

        std::shared_ptr<ResourceNode> const& getUserQueue() const
        {
            return userQueue;
        }

        T& get()
        {
            if(std::holds_alternative<std::reference_wrapper<T>>(storage))
            {
                return std::get<std::reference_wrapper<T>>(storage).get();
            }
            else
            {
                return std::get<T>(storage);
            }
        }

        T const& get() const
        {
            if(std::holds_alternative<std::reference_wrapper<T>>(storage))
            {
                return std::get<std::reference_wrapper<T>>(storage).get();
            }
            else
            {
                return std::get<T>(storage);
            }
        }

        // Read accessor
        ResourceAccess<Resource const> rg_read() const
        {
            return {*this, AccessMode::Read};
        }

        // Write  accessor
        ResourceAccess<Resource> rg_write()
        {
            return {*this, AccessMode::Write};
        }
    };


} // namespace rg
