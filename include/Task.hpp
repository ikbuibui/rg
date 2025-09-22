#pragma once

#include "Context.hpp"
#include "DispatchAwaiter.hpp"
#include "GetAwaiter.hpp"
#include "SharedCoroutineHandle.hpp"
#include "TaskPromise.hpp"
#include "ThreadPool.hpp"

#include <atomic>
#include <functional>
#include <type_traits>
#include <utility>

namespace rg
{

    template<bool synchronous = false, bool finishedOnReturn = false, typename Callable, typename... Args>
    auto dispatch_task(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args);

    // parser coroutine return type
    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    // TODO can i hold T as non optional, maybe if it is default constructible
    // [[nodiscard("The handle is required to get() the return value of the task")]]
    template<typename T, typename TPromise = DefaultPromise<T>>
    struct Task
    {
        template<typename U>
        friend struct InitTask;

        template<typename U, bool synchronous, bool finishedOnReturn>
        friend struct DispatchAwaiter;

        template<typename... TArgs>
        friend struct BarrierAwaiter;

        template<bool synchronous, bool finishedOnReturn, typename Callable, typename... Args>
        friend auto dispatch_task(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args);

        using promise_type = TPromise;

        explicit Task(SharedCoroutineHandle const& h) noexcept : coro(h)
        {
        }

        explicit Task() noexcept : coro()
        {
        }

        Task(Task const& x) = delete;

        Task(Task&& x) noexcept : coro{std::move(x.coro)}
        {
            x.isMoved = true;
        }

        Task& operator=(Task const& x) = delete;

        Task& operator=(Task&& x) noexcept
        {
            coro = std::move(x.coro);
            x.isMoved = true;
            return *this;
        }

        ~Task() noexcept
        {
            if(!isMoved && coro)
            {
                coro.promise<promise_type>().coroOutsideTask = false;
            }
        }

        auto get() -> GetAwaiter<promise_type>
        {
            // moved coro, calling get again is an error
            auto awaiter = GetAwaiter<promise_type>{std::move(coro)};
            // task holds self. Get called.
            // Set coro to nullptr.
            // TODO get cannot be called twice
            // make sure coro isnt used again by the handle. Either destroyed or owned by the execution space
            return awaiter;
        }

    private:
        SharedCoroutineHandle coro;
        bool isMoved = false;
    };

    template<typename T>
    concept IsTask = traits::is_specialization_of_v<std::remove_cvref_t<T>, Task>;

    template<typename Callable, typename... Args>
    concept ReturnsTask = requires(Callable&& func, Args&&... args) {
        // Check that the callable can be invoked with the given arguments
        // and that its return type (after removing cv-ref qualifiers) is a specialization of rg::Task
        { std::invoke_result_t<Callable, Args...>() } -> IsTask;
    };

    // Helper lambda to transform each accessHandle
    auto transform_resource(auto&& arg) -> decltype(auto)
    {
        if constexpr(IsTransformResourceAccess<std::decay_t<decltype(arg)>>)
        {
            return arg();
        }
        else
        {
            return std::forward<decltype(arg)>(arg);
        }
    };

    // TODO add requires ReturnsTask<Callable, Args...>
    template<bool synchronous, bool finishedOnReturn, typename Callable, typename... Args>
    auto dispatch_task(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args)
    {
        ctx.poolPtr = pool_p;
        auto handle = std::invoke(std::forward<Callable>(task), std::move(ctx), transform_resource(args)...);

        // register to the resources
        // if it is a transform resource, call transform on it
        constexpr uint16_t resource_counter = (static_cast<uint16_t>(IsResourceAccess<Args>) + ... + 0);
        auto& handlePromise = handle.coro.template promise<typename decltype(handle)::promise_type>();
        auto& resourceUsage = handlePromise.resourceUsage;
        auto& waitCounter = handlePromise.waitCounter;

        resourceUsage.reserve(resource_counter);
        waitCounter.fetch_add(resource_counter, std::memory_order_relaxed);
        // Register task to resources
        // Fold expression only for handles satisfying HasAccessType
        (...,
         (
             [&](auto& arg)
             {
                 if constexpr(IsResourceAccess<decltype(arg)>)
                 {
                     resourceUsage.emplace_back(
                         &arg.resource.getResNode().userQueue,
                         arg.resource.getResNode().userQueue.add_task(
                             {handle.coro.template get_coroutine_handle<typename decltype(handle)::promise_type>(),
                              arg.getAccessMode(),
                              &waitCounter,
                              ctx.poolPtr}));
                 }
             }(args)));

        return DispatchAwaiter<decltype(handle), synchronous, finishedOnReturn>{std::move(handle)};
    }

    template<bool synchronous = false, bool finishedOnReturn = false, typename Callable, typename... Args>
    auto dispatch_task(Context ctx, Callable&& task, Args&&... args)
    {
        return dispatch_task<synchronous, finishedOnReturn>(
            ctx.poolPtr,
            ctx,
            std::forward<Callable>(task),
            std::forward<Args>(args)...);
    }

} // namespace rg
