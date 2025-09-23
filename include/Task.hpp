#pragma once

#include "Context.hpp"
#include "GetAwaiter.hpp"
#include "SharedCoroutineHandle.hpp"
#include "TaskPromise.hpp"
#include "ThreadPool.hpp"

#include <type_traits>
#include <utility>

namespace rg
{

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

        template<typename Callable, typename... Args>
        friend auto invoke_register(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args);

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


} // namespace rg
