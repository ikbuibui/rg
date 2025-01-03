#pragma once

#include "resources.hpp"
#include "waitCounter.hpp"

#include <cassert>
#include <cmath>
#include <coroutine>
#include <cstdint>
#include <utility>

namespace rg
{
    template<typename T, bool Synchronous = false, bool finishedOnReturn = false>
    struct DispatchAwaiter;

    template<typename T, bool finishedOnReturn>
    struct DispatchAwaiter<T, true, finishedOnReturn>
    {
        T handle;
        bool resourcesReady;

        DispatchAwaiter(T&& handleObj, bool resReady) : handle{std::move(handleObj)}, resourcesReady{resReady}
        {
        }

        // always suspend
        bool await_ready() const noexcept
        {
            return false;
        }

        //  resources are not ready. add this continuation to handle and suspend
        template<typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            handle.coro.template promise<typename T::promise_type>().continuationHandle = h;
            uint32_t expectedState = 1;
            handle.coro.template promise<typename T::promise_type>().workingState.compare_exchange_strong(
                expectedState,
                2);
            // we are responsible to execute the task
            if(resourcesReady)
            {
                return handle.coro.get_coroutine_handle();
            }
            // task was blocked initially and was asynchronously executed
            else
            {
                // task was done before continuation handle was added
                if(expectedState == 0)
                {
                    return h;
                }
                else
                {
                    return std::noop_coroutine();
                }
            }
        }

        // passes the return object
        // TODO add return type
        auto await_resume() const noexcept
        {
            // return value
            return handle.coro.template promise<typename T::promise_type>().result.value();
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
        // using ResourceAccessList
        //     = TypeList<ResourceAccess<ResourceHandles::resource_id, typename ResourceHandles::access_type>...>;
        // takes ownership of the handle, and passes it on in await resume
        T handle;
        bool resourcesReady;

        DispatchAwaiter(T&& handleObj, bool resReady) : handle{std::move(handleObj)}, resourcesReady{resReady}
        {
        }

        bool await_ready() const noexcept
        {
            // suspend if resources ready, carry on continuation without suspend if not ready
            // suspend is false, dont suspend is true
            return !resourcesReady;
        }

        template<typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            // save here, as after dispatching self to the threadpool, this awaiter object (holding handle) may be
            // destroyed, then the return statement would be use after free
            auto resume_ready_handle = handle.coro.get_coroutine_handle();
            auto pool_p = h.promise().pool_p;
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
        auto await_resume() const noexcept
        {
            // return value
            return handle;
        }
    };

    // is called with co_await and it thus gives access to awaitable to the calling coro
    // awaitable should store a future and its promise should be set when the coroutine handle is finished
    // executing when is the awaitable destroyed, where to hold the memory of the promise template<typename T>
    // callable must be coroutine returning task type
    // args must be resources
    //
    // TODO pass the pool to the task
    // TODO pass in a way that i dont have to store stuff and make copies
    // Callable is a Task
    // thread_local static uint counter = 0;

    template<bool Synchronous = false, bool finishedOnReturn = false, typename Callable, typename... ResourceAccess>
    auto dispatch_task(Callable&& callable, ResourceAccess&&... accessHandles)
    {
        // TODO bind resources with restrictions applied
        // TODO Think about copies, references and lifetimes
        // std::cout << "Counter for thread " << std::this_thread::get_id() << " is " << counter++ << std::endl;

        uint16_t resource_counter = 0;
        // Helper lambda to process each accessHandle
        auto process_handle = [&resource_counter](auto&& handle) -> decltype(auto)
        {
            if constexpr(HasAccessType<std::decay_t<decltype(handle)>>)
            {
                resource_counter++;
                return handle.get(); // Call get() for types with access type
            }
            else
            {
                return std::forward<decltype(handle)>(handle); // Pass other types directly
            }
        };

        // create the awaitabletask coroutine.
        auto handle = std::invoke(
            std::forward<Callable>(callable),
            process_handle(std::forward<ResourceAccess>(accessHandles))...);

        // can access coro because it this function is a friend
        auto& handlePromise = handle.coro.template promise<typename decltype(handle)::promise_type>();
        auto& resourceNodes = handlePromise.resourceNodes;
        // this reserves too large a space, not all accessHandles are resources
        // resourceNodes.reserve(sizeof...(accessHandles));
        resourceNodes.reserve(resource_counter);

        // Register task to resources
        // Fold expression only for handles satisfying HasAccessType
        (...,
         (
             [&resourceNodes, &handle, &handlePromise](auto const& accessHandle)
             {
                 if constexpr(HasAccessType<std::decay_t<decltype(accessHandle)>>)
                 {
                     resourceNodes.push_back(accessHandle.getUserQueue());

                     accessHandle.getUserQueue()->add_task(
                         {handle.coro.get_coroutine_handle(),
                          typename std::decay_t<decltype(accessHandle)>::access_type{},
                          &handlePromise.waitCounter});
                 }
             }(std::forward<ResourceAccess>(accessHandles))));

        // task is ready to be eaten after fetch sub.
        // This is to make sure all resources are registered before someone deregistering sends this to readyQueue
        // If it returns INVALID_WAIT_STATE, then resource are ready and we are responsible to consume it
        auto wc = handlePromise.waitCounter.fetch_sub(INVALID_WAIT_STATE);
        bool resReady = (wc == INVALID_WAIT_STATE);

        // bool resReady = (handlePromise.waitCounter.fetch_sub(INVALID_WAIT_STATE) == INVALID_WAIT_STATE);

        // std::cout << "wc " << wc << " resources ready?" << resReady << std::endl;
        // auto bound_callable = std::bind_front(std::forward<Callable>(callable), handles.obj...);

        //  Bind the resources as arguments to the callable/coroutine
        // Deduce the return type of the callable with bound arguments
        // using ReturnType = decltype(callable(std::forward<Args>(res)...).get());

        // if(resourcesReady)
        //  return awaitable that puts the continuation on the stack, executes the task
        //  returns returnHandleObject
        // else
        //  return awaitable that puts the task on the st tack, resumes continuation on the stack
        //  returns task returnHandleObject

        // final_suspend removes from task and notifies
        return DispatchAwaiter<decltype(handle), Synchronous, finishedOnReturn>{std::move(handle), resReady};
    }

    // TODO Dispatch for
    // - for short tasks, which dont need to place continuation to pool queue
    //   if ready, do and continue; else, place task in waiters
    // - for callables that are not rg::tasks, and dont need their own tasks space


} // namespace rg
