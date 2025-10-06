#pragma once

#include "resourceTransforms.hpp"
#include "traits.hpp"

#include <cassert>
#include <cstdint>
#include <utility>

namespace rg
{
    enum class AccessMode : uint8_t
    {
        Read,
        Write,
        AAdd,
        AMul,
        Uninitialized
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

    template<typename T>
    concept HasAccessType = requires { typename T::access_type; };

    template<typename T>
    concept NotAccessType = !HasAccessType<T>;

    // TODO think one for IOResources and another for others?
    bool is_serial_access(AccessMode const a, AccessMode const b)
    {
        assert(a != AccessMode::Uninitialized && b != AccessMode::Uninitialized && "Access mode is not initialized");

        return (a != AccessMode::Read || b != AccessMode::Read) && (a != AccessMode::AAdd || b != AccessMode::AAdd)
               && (a != AccessMode::AMul || b != AccessMode::AMul);
    }

    struct TaskData;

    // This is the type which we get back from .read() . write() on a resource
    // This is the type which the user is provided with inside a task
    // The user doesnt need to know the access mode. The access mode is not needed inside the task. Only for
    // registration
    template<typename TResource>
    class ResourceAccess
    {
    public:
        // TODO make private

        TResource resource;

        AccessMode access{AccessMode::Uninitialized};

    public:
        using access_type = AccessMode;

        // get the memory of taskData from the resourceTaskQueue
        ResourceAccess(TResource const& r, AccessMode accessMode) noexcept : resource(r), access(accessMode)
        {
        }

        ResourceAccess(TResource&& r, AccessMode accessMode) noexcept : resource(std::move(r)), access(accessMode)
        {
        }

        uint32_t getID() const
        {
            return resource.getResNode().getId();
        }

        AccessMode getAccessMode()
        {
            return access;
        }

        decltype(auto) get() const
        {
            return resource.get();
        }

        decltype(auto) get()
        {
            return resource.get();
        }

        decltype(auto) operator*() const
        {
            return get();
        }

        decltype(auto) operator*()
        {
            return get();
        }

        decltype(auto) operator->() const
        {
            return std::addressof(get());
        }

        decltype(auto) operator->()
        {
            return std::addressof(get());
        }

        // using ResourceNodeType = typename TResource::ResNodeType;

        // ResourceNodeType& getResNode()
        // {
        //     return resource.getResNode();
        // }

        // ResourceNodeType const& getResNode() const
        // {
        //     return resource.getResNode();
        // }
    };

    template<typename TResource, typename Transform>
    class TransformResourceAccess : public ResourceAccess<TResource>
    {
    public:
        // TODO make private
        Transform transform{default_transformer};

    public:
        // get the memory of taskData from the resourceTaskQueue
        TransformResourceAccess(TResource const& r, AccessMode accessMode, Transform t) noexcept
            : ResourceAccess<TResource>(r, accessMode)
            , transform(t)
        {
        }

        TransformResourceAccess(TResource&& r, AccessMode accessMode, Transform t) noexcept
            : ResourceAccess<TResource>(std::move(r), accessMode)
            , transform(t)
        {
        }

        auto operator()(auto&& arg) const -> decltype(auto)
        {
            return transform(std::forward<decltype(arg)>(arg));
        }
    };

    // mixing struct, TRes will inherit from IOAccess<TRes>,kind of CRTP style.
    template<typename TResource>
    struct IOAccess
    {
        // TODO rename rg_read and rg_write
        // TODO add &,&& overloads to move resource, i.e. self if read and write are called on a temporary resource
        // Read accessor
        ResourceAccess<TResource const> rg_read() const
        {
            return {*static_cast<TResource const*>(this), AccessMode::Read};
        }

        // Write  accessor
        ResourceAccess<TResource> rg_write()
        {
            return {*static_cast<TResource*>(this), AccessMode::Write};
        }

        template<typename Transform>
        TransformResourceAccess<TResource const, Transform> rg_read(Transform&& t) const
        {
            return {*static_cast<TResource const*>(this), AccessMode::Read, std::forward<Transform>(t)};
        }

        template<typename Transform>
        TransformResourceAccess<TResource, Transform> rg_write(Transform&& t)
        {
            return {*static_cast<TResource*>(this), AccessMode::Write, std::forward<Transform>(t)};
        }
    };

    template<typename T>
    concept IsResourceAccess = traits::is_specialization_of_v<std::remove_cvref_t<T>, ResourceAccess>;

    template<typename T>
    concept IsTransformResourceAccess
        = traits::is_specialization_of_v<std::remove_cvref_t<T>, TransformResourceAccess>;

} // namespace rg
