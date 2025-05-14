#pragma once

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

        AccessMode&& moveAccessMode()
        {
            return std::move(access);
        }

        auto const& get() const
        {
            return resource.get();
        }

        auto& get()
        {
            return resource.get();
        }

        auto const& operator*() const
        {
            return get();
        }

        auto& operator*()
        {
            return get();
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
    };

    template<typename T>
    concept IsResourceAccess = traits::is_specialization_of_v<std::remove_cvref_t<T>, ResourceAccess>;


} // namespace rg
