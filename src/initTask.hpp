#pragma once

#include "ThreadPool.hpp"
#include "dispatchTask.hpp"

#include <coroutine>
#include <cstdint>
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
            ThreadPool* pool_p;
            std::optional<T> result;
            std::mutex mtx;
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
                std::cout << "initial suspend called" << std::endl;

                return {};
            }

            std::suspend_always final_suspend() noexcept
            {
                std::cout << "final suspend called" << std::endl;
                // notify thart work is finished here
                // finalize
                // std::lock_guard<std::mutex> lock(mtx);
                // cv.notify_one();
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

                // pass in the pool ptr
                handle.coro.promise().pool_p = pool_p;
                // Use task space and register handle to resources
                // resources Ready based on reutrn value of register to resources
                bool resourcesReady = true;
                // if(resourcesReady)
                //   return awaiter that suspends, adds continuation to stack, and executes task
                // elseif resources not ready
                //   add handle to waiting task map, return awaiter that suspend never (executes the continuation)

                if(!resourcesReady)
                {
                    // emplace the to be dispatched coro to the waiting_tasks_map, and continue exec
                    pool_p->dispatch_task(handle.coro); // TODO FIX! THIS IS NOT THE WAITING TASKS MAP
                }

                return DispatchAwaiter{resourcesReady, std::move(handle)};
            }
        };

        InitTask(std::coroutine_handle<promise_type> h) : coro{h}
        {
            std::cout << "init return object created" << std::endl;
        }

        ~InitTask()
        {
            std::cout << "destroy orcHandle" << std::endl;
        }

        std::optional<T> get()
        {
            // sleep till final suspend notifies
            // coro.done() is check for spurious wakeups
            // can also spin on coro.done, may be easier
            // while(!coro.done){} return coro.promise().result;
            std::unique_lock<std::mutex> lock(coro.promise().mtx);
            coro.promise().cv.wait(lock, coro.done());
            // todo make this exception safe
            auto result = coro.promise().result.value();
            coro.destroy();
            return result;
        }

        std::coroutine_handle<promise_type> coro;
    };

    // TODO also make initTask and orchestrate for void

} // namespace rg
