#pragma once


#include "ThreadPool.hpp"

#include <coroutine>
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

        constexpr void await_suspend(std::coroutine_handle<> h) const noexcept
        {
            pool->dispatch_task(h);
        }

        constexpr void await_resume() const noexcept
        {
        }
    };

    // parser coroutine return type
    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    template<typename T>
    struct Task
    {
        struct promise_type
        {
            ThreadPool* pool;
            std::optional<T> result;

            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            exec_inplace_if initial_suspend() noexcept
            {
                // register to resources
                auto resourcesReady = true;

                // emplace continuation to stack
                pool->dispatch_task(std::coroutine_handle<promise_type>::from_promise(*this));

                return {resourcesReady};
            }

            std::suspend_always final_suspend() noexcept
            {
                return {};
            }

            void unhandled_exception()
            {
                std::terminate();
            }

            template<typename U>
            void return_value(U&& value)
            {
                result = std::forward<U>(value);
            }
        };

        std::coroutine_handle<promise_type> coro;

        explicit Task(std::coroutine_handle<promise_type> h) : coro(h)
        {
        }

        ~Task()
        {
            if(coro)
                coro.destroy();
        }

        Task(Task const&) = delete;
        Task(Task&&) = delete;
        Task& operator=(Task const&) = delete;
        Task& operator=(Task&&) = delete;

        T get()
        {
            return coro.promise().result.value();
        }
    };

    // is called with co_await and it returns an awaitable to the calling coro
    // awaitable should store a future and its promise should be set when the coroutine handle is finished executing
    // when is the awaitable destroyed, where to hold the memory of the promise
    template<typename T>
    auto dispatch_task(std::invocable<> auto x) -> Task<T>
    {
        // execute
        x();
    }

} // namespace rg
