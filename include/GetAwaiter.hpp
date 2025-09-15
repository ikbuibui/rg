#pragma once

#include "SharedCoroutineHandle.hpp"

#include <atomic>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace rg
{
    // When get is called on a handle return object
    // if coro is done - dont suspend - await ready true, and return promise value in await resume
    // if coro is not done - suspend (go to caller, if it is main, skip over everything, this will be resumed by the
    // pool when coro is done), add to waiter queue (worker will loop over this queue and check if done) Add promise
    // type for this awaiter
    // for void tasks this waits for task to complete
    // template on awaited promise to see if it holds a task space
    template<typename AwaitedPromise>
    struct GetAwaiter
    {
        SharedCoroutineHandle coro;

        bool await_ready() const noexcept
        {
            // The task is ready without suspending if its working state is 0 (done), else suspend.
            return coro.promise<AwaitedPromise>().workingState.load(std::memory_order_acquire) == 0;
        }

        // has a lock to prevent final suspend of coro being done when await suspend is being called
        template<typename ContPromise>
        bool await_suspend(std::coroutine_handle<ContPromise> h) noexcept
        {
            // If coro not done, add h to its waiter handle and it will be done in final suspend
            auto& promise = coro.promise<AwaitedPromise>();
            // if coro is done, we can simply resume h on final suspend
            promise.continuationHandle = h;

            // Atomically transition from 'running' (1) to 'continuation attached' (2).
            // If the task finished concurrently (state became 0), the CAS will fail.
            uint32_t expectedState = 1;
            // Suspend the caller if state was changed, else task was already completed and we dont suspend.
            return promise.workingState.compare_exchange_strong(expectedState, 2, std::memory_order_acq_rel);
        }

        // will only be called after the task is done
        auto await_resume() const noexcept
        {
            auto& promise = coro.promise<AwaitedPromise>();
            promise.coroOutsideTask = false;

            // Return the result for non-void tasks.
            if constexpr(!std::is_void_v<typename AwaitedPromise::return_type>)
            {
                return std::move(promise.result);
            }
        }
    };
} // namespace rg
