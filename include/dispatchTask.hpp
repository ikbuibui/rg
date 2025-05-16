#pragma once

#include "waitCounter.hpp"

#include <atomic>
#include <cassert>
#include <coroutine>
#include <utility>

namespace rg
{
    /**
     * @brief DispatchAwaiter is a struct template that manages the suspension and resumption of coroutines.
     *
     * @tparam T The type of the coroutine handle.
     * @tparam Synchronous A boolean indicating if the continuation should striclty happen after the passed in handle
     * @tparam finishedOnReturn A boolean indicating if the task guarantees that it, including its subtasks is finished
     * when it returns.
     */
    template<typename T, bool Synchronous = false, bool finishedOnReturn = false>
    struct DispatchAwaiter;

    /**
     * @brief Specialization of DispatchAwaiter for synchronous executions.
     */
    template<typename T, bool finishedOnReturn>
    struct DispatchAwaiter<T, true, finishedOnReturn>
    {
        T handle;

        DispatchAwaiter(T&& handleObj) : handle{std::move(handleObj)}
        {
        }

        // always suspend
        bool await_ready() const noexcept
        {
            return false;
        }

        template<typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            auto& handle_promise = handle.coro.template promise<typename T::promise_type>();
            handle_promise.continuationHandle = h;

            handle_promise.workingState.store(2);

            // can access coro because it this function is a friend
            auto& waitCounter = handle.coro.template promise<typename decltype(handle)::promise_type>().waitCounter;

            // task is ready to be eaten after fetch sub.
            // This is to make sure all resources are registered before someone deregistering sends this to readyQueue
            // If it returns INVALID_WAIT_STATE, then resource are ready and we are responsible to consume it
            auto wc = waitCounter.fetch_sub(INVALID_WAIT_STATE, std::memory_order_acq_rel);
            bool resourcesReady = (wc == INVALID_WAIT_STATE);

            // we are responsible to execute the task
            if(resourcesReady)
            {
                return handle.coro.get_coroutine_handle();
            }
            // task was blocked initially and was asynchronously executed
            else
            {
                // task was done before continuation handle was added
                return std::noop_coroutine();
            }
        }

        auto await_resume() const noexcept
        {
            if constexpr(std::is_void_v<typename T::promise_type::return_type>)
            {
                return;
            }
            else
            {
                return handle.coro.template promise<typename T::promise_type>().result;
            }
        }
    };

    // if(resourcesReady)
    //   return awaiter that suspends, adds continuation to stack, and executes task
    // elseif resources not ready
    //   already added handle to waiting task map/or set waiting atomic value, return awaiter that suspend never
    //   (executes the continuation)
    // TODO switch to IsResourceAccess
    template<typename T, bool finishedOnReturn>
    struct DispatchAwaiter<T, false, finishedOnReturn>
    {
        // takes ownership of the handle, and passes it on in await resume
        T handle;

        DispatchAwaiter(T&& handleObj) : handle{std::move(handleObj)}
        {
        }

        bool await_ready() const noexcept
        {
            // can access coro because it this function is a friend
            auto& waitCounter = handle.coro.template promise<typename decltype(handle)::promise_type>().waitCounter;

            // task is ready to be eaten after fetch sub.
            // This is to make sure all resources are registered before someone deregistering sends this to readyQueue
            // If it returns INVALID_WAIT_STATE, then resource are ready and we are responsible to consume it
            auto wc = waitCounter.fetch_sub(INVALID_WAIT_STATE, std::memory_order_acq_rel);
            bool const resourcesReady = (wc == INVALID_WAIT_STATE);
            // suspend if resources ready, carry on continuation without suspend if not ready
            // suspend is false, dont suspend is true
            return !resourcesReady;
        }

        template<typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            // save here, as after dispatching continuation to the threadpool, this awaiter object (holding handle) may
            // be destroyed, then the return statement would be use after free
            auto const resume_ready_handle = handle.coro.get_coroutine_handle();
            auto const pool_p = h.promise().pool_p;
            // suspend only called when resources are ready
            // assert(resourcesReady);
            // emplace continuation to stack
            // TODO make sure the promise of the continuation can access the return type of
            pool_p->addTask(h);

            // execute the coroutine
            // USING THIS is dangerous cont may be finished and destroy this awitable object
            return resume_ready_handle;
        }

        // template<typename TPromise>
        // bool await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        // {
        //     // save here, as after dispatching self to the threadpool, this awaiter object (holding handle) may be
        //     // destroyed, then the return statement would be use after free
        //     auto resume_ready_handle = handle.coro.get_coroutine_handle();
        //     // suspend only called when resources are ready
        //     // assert(resourcesReady);
        //     // emplace continuation to stack
        //     // TODO make sure the promise of the continuation can access the return type of
        //     h.promise().pool_p->addReadyTask(resume_ready_handle);

        //     // execute the coroutine
        //     // USING THIS is dangerous cont may be finished and destroy this awitable object
        //     return false;
        // }

        // passes the return object
        // TODO add return type
        auto await_resume() noexcept
        {
            // return value
            return std::move(handle);
        }
    };

    // TODO Dispatch for
    // - for short tasks, which dont need to place continuation to pool queue
    //   if ready, do and continue; else, place task in waiters
    // - for callables that are not rg::tasks, and dont need their own tasks space
    namespace detail
    {
        template<typename T>
        struct is_dispatch_awaiter_impl : std::false_type
        {
        };

        // Specialization for DispatchAwaiter
        template<typename WrappedType, bool IsSync, bool IsFinishedOnRet>
        struct is_dispatch_awaiter_impl<DispatchAwaiter<WrappedType, IsSync, IsFinishedOnRet>> : std::true_type
        {
        };
    } // namespace detail

    // Concept definition
    template<typename MaybeAwaiter>
    concept IsDispatchAwaiter = detail::is_dispatch_awaiter_impl<std::decay_t<MaybeAwaiter>>::value;

} // namespace rg
