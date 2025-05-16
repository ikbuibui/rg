#pragma once

#include "Resource.hpp"
#include "Task.hpp"
#include "ThreadPool.hpp"

#include <coroutine>
#include <type_traits>
#include <vector>

namespace rg
{
    template<typename T>
    concept ResourceContainer = requires(T container) {
        typename T::value_type;
        requires IsResource<typename T::value_type>;
        { std::begin(container) } -> std::input_iterator;
        { std::end(container) } -> std::input_iterator;
    };

    template<typename... TArgs>
    struct BarrierAwaiter
    {
        // TODO make sure holding references is fine
        // using ArgTuple = std::tuple<std::remove_cvref_t<TArgs>...>;
        using ArgTuple = std::tuple<std::reference_wrapper<std::remove_cvref_t<TArgs>>...>;
        ArgTuple resArgs;

        // BarrierAwaiter(TArgs... res) : resArgs(std::move(res)...)
        // {
        // }
        BarrierAwaiter(TArgs&... args) requires((IsResource<TArgs> && ...))
            : resArgs(std::ref(args)...)
        {
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
            auto& cont_promise = h.promise();
            auto ctx = Context(cont_promise.self, cont_promise.pool_p);
            // save here, as after dispatching continuation to the threadpool, this awaiter object (holding handle) may
            // be destroyed, then the return statement would be use after free
            auto handle = std::apply(
                [ctx, h](auto&... args)
                {
                    // TODO move args in. Make rg write move res into resAccess
                    return [](Context ctx, std::coroutine_handle<TPromise> h, auto...) -> Task<void>
                    {
                        auto& cont_promise = h.promise();
                        ctx.poolPtr->addBarrier(cont_promise.self, cont_promise.coroOutsideTask);
                        co_return;
                    }(ctx, h, args.get().rg_write()...);
                },
                resArgs);

            auto const resume_ready_handle = handle.coro.get_coroutine_handle();

            auto& handle_promise = handle.coro.template promise<typename decltype(handle)::promise_type>();
            // can access coro because it this function is a friend
            auto& waitCounter = handle_promise.waitCounter;

            // task is ready to be eaten after fetch sub.
            // This is to make sure all resources are registered before someone deregistering sends this to readyQueue
            // If it returns INVALID_WAIT_STATE, then resource are ready and we are responsible to consume it
            auto wc = waitCounter.fetch_sub(INVALID_WAIT_STATE, std::memory_order_acq_rel);
            bool resourcesReady = (wc == INVALID_WAIT_STATE);

            // we are responsible to execute the task
            if(resourcesReady)
            {
                return resume_ready_handle;
            }
            // task will be asynchronously executed
            else
            {
                return std::noop_coroutine();
            }
        }

        void await_resume() const noexcept
        {
        }
    };

    // Doesnt accept rvalues as they will dangle as barrier only stores references
    auto barrier(IsResource auto&... resArgs)
    {
        // the task adds dependency on resources, a seperate block on
        return BarrierAwaiter(resArgs...);
    }

} // namespace rg
