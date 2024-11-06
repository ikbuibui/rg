#pragma once

#include "ThreadPool.hpp"

#include <cassert>
#include <coroutine>
#include <functional>
#include <optional>
#include <utility>

namespace rg
{

    // TODO add promise type to coroutine handles
    struct exec_inplace_if
    {
        bool ready;

        constexpr bool await_ready() const noexcept
        {
            return ready;
        }

        template<typename TPromise>
        constexpr void await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            h.promise().pool_p->dispatch_task(h);
        }

        constexpr void await_resume() const noexcept
        {
        }
    };

    // Used as final suspend awaitable, as if there is a value to be returned, this will suspend the coroutine
    // thus keeping its frame alive, else dont suspend, which will destroy the coroutine
    struct suspend_if
    {
        bool condition;

        constexpr bool await_ready() const noexcept
        {
            return !condition;
        }

        constexpr void await_suspend(std::coroutine_handle<>) const noexcept
        {
        }

        constexpr void await_resume() const noexcept
        {
        }
    };

    template<typename Callable, typename... Args>
    struct DeferredCallable
    {
        // Constructor to store the callable and its arguments
        DeferredCallable(Callable&& callable, Args&&... args)
            : callable_(std::forward<Callable>(callable))
            , args_(std::forward<Args>(args)...)
        {
        }

        // Invokes the callable with the stored arguments
        auto operator()()
        {
            return std::apply(callable_, args_);
        }

    private:
        Callable callable_;
        std::tuple<Args...> args_;
    };

    template<typename THandle>
    struct DispatchAwaiter
    {
        bool resourcesReady = false;
        // takes ownership of the handle, and passes it on in await resume
        THandle taskHandleObj;

        DispatchAwaiter(bool resourcesReady, THandle&& handle)
            : resourcesReady{resourcesReady}
            , taskHandleObj{std::forward<THandle>(handle)}
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
            // suspend only called when resources are ready
            // assert(resourcesReady);
            // emplace continuation to stack
            // TODO make sure the promise of the continuation can access the return type of
            std::cout << "push continuation onto stack" << std::endl;
            h.promise().pool_p->dispatch_task(h);

            // execute the coroutine
            return taskHandleObj.coro;
        }

        // passes the return object
        // TODO add return type
        auto await_resume() const noexcept
        {
            // return value
            return taskHandleObj;
        }
    };

    // parser coroutine return type
    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    // TODO can i hold T as non optional, maybe if it is default constructible
    template<typename T>
    struct Task
    {
        struct promise_type
        {
            ThreadPool* pool_p;
            std::optional<T> result;

            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            // required to suspend as handle coroutine is created in dispactch task
            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            // do suspend if, if returns void, suspend never (which will destroy the coroutine), if returns a value,
            // suspend and destroy in get
            suspend_if final_suspend() noexcept
            {
                // Deregister from resources
                // Notify people
                // conditionally destroy memory
                std::cout << "task result has value " << result.has_value() << std::endl;

                return {result.has_value()};
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

        std::coroutine_handle<promise_type> coro;

        explicit Task(std::coroutine_handle<promise_type> h) : coro(h)
        {
        }

        bool resourcesReady() const
        {
            return coro.promise().resourcesReady;
        }

        T get()
        {
            return coro.promise().result.value();
        }
    };

    // is called with co_await and it thus gives access to awaitable to the calling coro
    // awaitable should store a future and its promise should be set when the coroutine handle is finished
    // executing when is the awaitable destroyed, where to hold the memory of the promise template<typename T>
    // callable must be coroutine returning task type
    // args must be resources
    //
    // TODO pass the pool to the task
    template<typename Callable, typename... Args>
    auto dispatch_task(Callable&& callable, Args&&... res)
    {
        // TODO bind resources with restrictions applied
        // bind res as args of coroutine
        // Deduce the return type of the callable with bound arguments
        // using ReturnType = decltype(callable(std::forward<Args>(res)...).get());

        //  Bind the resources as arguments to the callable/coroutine
        // auto bound_callable = std::bind_front(std::forward<Callable>(callable), std::forward<Args>(res)...);

        // auto handle = callable(std::forward<Args>(res)...);

        // if(resourcesReady)
        //  return awaitable that puts the continuation on the stack, executes the task
        //  returns returnHandleObject
        // else
        //  return awaitable that puts the task on the st tack, resumes continuation on the stack
        //  returns task returnHandleObject


        // execute
        // x();
        //
        // final_suspend removes from task and notifies
        // return DispatchAwaiter{handle};
        //
        return DeferredCallable{std::forward<Callable>(callable), std::forward<Args>(res)...};
    }

} // namespace rg
