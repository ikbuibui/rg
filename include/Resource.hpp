#pragma once

#include "ResourceAccess.hpp"
#include "ResourceTaskQueue.hpp"
#include "traits.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

namespace rg
{

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

    // ResourceHandle


    template<typename T>
    struct ResNode
    {
        T resource;
        // Unique identifier for the resource
        // Dont really need this
        uint32_t resource_uid;
        mutable ResourceTaskQueue userQueue{};

        // TODO store value here
        // construct by value and move in. Has by value container semantics
        // if the user wants to pass a reference, they can use a reference wrapper

        ResNode(T const& res) : resource{res}, resource_uid(GlobalIDGenerator::generate_id())
        {
        }

        ResNode(T&& res) : resource{std::move(res)}, resource_uid(GlobalIDGenerator::generate_id())
        {
        }

        ResNode() requires std::default_initializable<T>
            : resource{}
            , resource_uid(GlobalIDGenerator::generate_id())
        {
        }

        auto getId() const
        {
            return resource_uid;
        }
    };

    // has value semantics. If you want to store a reference, pass a std::ref
    template<typename T>
    class Resource : public IOAccess<Resource<T>>
    {
    public:
        // TODO change to private
        using ResNodeType = ResNode<T>;

    private:
        std::shared_ptr<ResNodeType> resNode;

    public:
        [[nodiscard]] Resource(T const& value) : resNode(std::make_shared<ResNodeType>(value))
        {
        }

        [[nodiscard]] Resource(T&& value) : resNode(std::make_shared<ResNodeType>(std::move(value)))
        {
        }

        template<typename U>
        requires(!std::same_as<std::remove_cvref_t<U>, Resource>)
        [[nodiscard]] explicit Resource(U&& value) : resNode(std::make_shared<ResNodeType>(std::forward<U>(value)))
        {
        }

        [[nodiscard]] Resource() requires std::default_initializable<T>
            : resNode(std::make_shared<ResNodeType>())
        {
        }

        // template<typename U>
        // requires std::convertible_to<U*, ResNodeType*>
        // Resource(Resource<U> const& other) : resNode(other.resNode)
        // {
        // }

        // template<typename U>
        // requires std::convertible_to<U*, ResNodeType*>
        // Resource(Resource<U>&& other) noexcept : resNode(std::move(other.resNode))
        // {
        // }

        ResNodeType& getResNode() const
        {
            return *resNode;
        }

        T& get() &
        {
            return resNode->resource;
        }

        T const& get() const&
        {
            return resNode->resource;
        }

        T&& get() &&
        {
            return std::move(resNode->resource);
        }

        T const&& get() const&&
        {
            return std::move(resNode->resource);
        }

        bool unique() const
        {
            return resNode.unique();
        }

        long use_count() const
        {
            return resNode.use_count();
        }
    };

    template<typename T>
    concept IsResource = traits::is_specialization_of_v<std::remove_cvref_t<T>, Resource>;


} // namespace rg
