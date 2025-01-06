#pragma once

#include "Task.hpp"
#include "ThreadPool.hpp"
#include "resources.hpp"
#include "traits.hpp"

#include <coroutine>
#include <functional>
#include <vector>

namespace rg
{
    template<typename T>
    concept ResourceContainer = requires(T container) {
        typename T::value_type;
        requires traits::is_specialization_of_v<Resource, typename T::value_type>;
        { std::begin(container) } -> std::input_iterator;
        { std::end(container) } -> std::input_iterator;
    };

    template<typename... ResArgs>
    struct BarrierAwaiter
    {
        std::tuple<std::reference_wrapper<ResArgs const>...> resources;

        BarrierAwaiter(ResArgs const&... args) : resources(std::ref(args)...)
        {
        }

        template<typename T>
        void processResource(Resource<T> const& resource, auto const& handle, auto& resourceNodes, auto& handlePromise)
        {
            auto const& userQueue = resource.getUserQueue();
            resourceNodes.push_back(userQueue);
            userQueue->add_task(
                {handle.coro.get_coroutine_handle(), access::write::access_type{}, &handlePromise.waitCounter});
        }

        // Process a container of resources
        template<ResourceContainer RC>
        void processResource(RC const& container, auto const& handle, auto& resourceNodes, auto& handlePromise)
        {
            for(auto const& resource : container)
            {
                processResource(resource, handle, resourceNodes, handlePromise);
            }
        }

        // always suspend
        bool await_ready() const noexcept
        {
            return false;
        }

        //  resources are not ready. add this continuation to handle and suspend
        template<typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> h) noexcept
        {
            // std::cout << "in barrier" << std::endl;

            auto handle = [](std::coroutine_handle<TPromise> continuation) -> rg::Task<void>
            {
                auto& promise = continuation.promise();
                promise.pool_p->barrier_queue.emplace_back(promise.self, promise.coroOutsideTask);
                co_return;
            }(h);

            // can access coro because it this function is a friend
            auto& handlePromise = handle.coro.template promise<typename decltype(handle)::promise_type>();

            // continuatiom handled by adding to barrier queue
            // handlePromise.continuationHandle = h;
            // handlePromise.workingState = 2;

            auto& resourceNodes = handlePromise.resourceNodes;
            resourceNodes.reserve(sizeof...(ResArgs));

            std::apply(
                [&resourceNodes, &handlePromise, &handle, this](auto const&... resources)
                {
                    ([&resources, &resourceNodes, &handlePromise, &handle, this]()
                     { processResource(resources.get(), handle, resourceNodes, handlePromise); },
                     ...);
                },
                resources);

            // task is ready to be eaten after fetch sub.
            // This is to make sure all resources are registered before someone deregistering sends this to readyQueue
            // If it returns INVALID_WAIT_STATE, then resource are ready and we are responsible to consume it
            auto wc = handlePromise.waitCounter.fetch_sub(INVALID_WAIT_STATE);
            bool resourcesReady = (wc == INVALID_WAIT_STATE);


            // we are responsible to execute the task
            if(resourcesReady)
            {
                return handle.coro.get_coroutine_handle();
            }
            // task was blocked initially and was/will be asynchronously executed
            else
            {
                return std::noop_coroutine();
            }
        }

        void await_resume() const noexcept
        {
        }
    };
} // namespace rg
