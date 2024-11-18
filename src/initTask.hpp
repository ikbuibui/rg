#pragma once

#include "ThreadPool.hpp"
#include "dispatchTask.hpp"

#include <coroutine>
#include <iostream>
#include <mutex>
#include <optional>

namespace rg
{
    // awaiter to place its own continuation to the pool stack and return to caller
    template<typename promise_type>
    struct DetachToPoolAwaiter
    {
        std::coroutine_handle<promise_type> h;

        // doesnt suspend the coroutine call
        bool await_ready() noexcept
        {
            return false;
        };

        void await_suspend(std::coroutine_handle<promise_type> h) noexcept
        {
            // add me to stack
            // and return to main
            std::cout << "handle to coro cont emplaced" << std::endl;
            h.promise().pool_p->emplace_init_frame(h);
        };

        void await_resume() noexcept {};
    };

    // parser coroutine return type

    // returns the value of the callable
    // I want to the awaiter of initial_suspend to put its handle to the handle stack
    // handle stack will be eaten by the pool
    // promise holds the pool ptr
    template<typename T>
    struct InitTask
    {
        struct promise_type
        {
            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};
            // does this need to be optional?
            std::optional<T> result = std::nullopt;
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            // std::atomic<uint32_t> waitCounter = 0;
            // if .get is called and this coro is not done, add waiter handle here to notify on final suspend
            // someone else waits for the completion of this task.
            std::coroutine_handle<> getWaiterHandle = nullptr;
            // mutex to synchronize final suspend and .get() waiting which adds a dependency
            std::mutex mtx;

            // Task space for the children of this task. Passed in its ptr during await transform
            ExecutionSpace rootSpace{};

            std::condition_variable cv;

            promise_type(ThreadPool* ptr) : pool_p{ptr}
            {
            }

            InitTask get_return_object()
            {
                return InitTask{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_never initial_suspend() noexcept
            {
                return {};
            }

            std::suspend_always final_suspend() noexcept
            {
                // notify thart work is finished here
                // finalize
                std::lock_guard<std::mutex> lock(mtx);
                cv.notify_one();
                return {};
            }

            void unhandled_exception()
            {
                std::terminate();
            }

            // allows conversions
            template<typename U>
            void return_value(U&& value)
            {
                result = std::forward<U>(value);
            }

            // TODO contrain args to resource concept
            template<typename Callable, typename... Args>
            auto await_transform(DeferredCallable<Callable, Args...> dc)
            {
                // create the task coroutine.
                auto handle = dc();

                // TODO can i merge this with init
                // init
                // pass in the parent task space
                handle.coro.promise().parentSpace_p = &rootSpace;
                // pass in the pool ptr
                handle.coro.promise().pool_p = pool_p;
                // Init over
                handle.coro.promise().space.pool_p = pool_p;
                rootSpace.pool_p = pool_p;

                // Register handle to all resources in the execution space
                auto wc = rootSpace.addDependencies(handle.coro, dc);
                handle.coro.promise().waitCounter = wc;
                std::cout << "value set in wait counter is " << wc << std::endl;
                // resources Ready based on reutrn value of register to resources or value of waitCounter
                bool resourcesReady = (wc == 0);

                // if(resourcesReady)
                //   return awaiter that suspends, adds continuation to stack, and executes task
                // elseif resources not ready
                //   task has been initialized with wait counter, waits for child notification to add to ready queue
                //   return awaiter that suspend never (executes the continuation)

                return DispatchAwaiter{resourcesReady, std::move(handle)};
            }

            template<typename U, typename AwaitedPromise>
            auto await_transform(GetAwaiter<U, AwaitedPromise> aw)
            {
                return aw;
            }
        };

        InitTask(std::coroutine_handle<promise_type> h) : coro{h}
        {
        }

        std::optional<T> get()
        {
            // sleep till final suspend notifies
            // coro.done() is check for spurious wakeups
            // can also spin on coro.done, may be easier
            // while(!coro.done){} return coro.promise().result;
            std::unique_lock<std::mutex> lock(coro.promise().mtx);
            coro.promise().cv.wait(lock, [this] { return coro.done(); });
            // todo make this exception safe
            auto result = coro.promise().result.value();
            // coro.destroy();
            // make sure coro isnt used again by the handle. Either destroyed or owned by the execution space
            coro = nullptr;
            return result;
        }

        std::coroutine_handle<promise_type> coro;
    };

    // TODO also make initTask and orchestrate for void

} // namespace rg
