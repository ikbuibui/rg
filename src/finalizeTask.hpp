#pragma once

#include "ThreadPool.hpp"

#include <coroutine>
#include <optional>

namespace rg
{

    template<typename T>
    struct FinalTask;

    // awaiter to place its own continuation to the pool stack
    template<typename T>
    struct MainResumerAwaiter
    {
        bool await_ready()
        {
            return false;
        };

        void await_suspend(std::coroutine_handle<typename FinalTask<T>::promise_type> h)
        {
            h.promise().pool_p->finalize(h);
        };

        void await_resume() {};
    };

    template<typename T>
    struct FinalTask
    {
        struct promise_type
        {
            ThreadPool* pool_p;
            std::optional<T> result;

            explicit promise_type(ThreadPool* ptr, T&& t) : pool_p{ptr}, result{std::forward<T>(t)}
            {
            }

            FinalTask get_return_object()
            {
                return {std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            MainResumerAwaiter<T> initial_suspend() noexcept
            {
                return {};
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

        FinalTask(std::coroutine_handle<promise_type> h) : coro(h)
        {
        }

        // can only be called once
        T get()
        {
            auto temp = std::move(coro.promise().result.value());
            coro.destroy();
            return temp;
        }
    };

    template<>
    struct FinalTask<void>
    {
        struct promise_type
        {
            ThreadPool* pool_p;

            explicit promise_type(ThreadPool* ptr) : pool_p{ptr}
            {
            }

            FinalTask get_return_object()
            {
                return {};
            }

            MainResumerAwaiter<void> initial_suspend() noexcept
            {
                return {};
            }

            std::suspend_always final_suspend() noexcept
            {
                return {};
            }

            void unhandled_exception()
            {
                std::terminate();
            }

            void return_void()
            {
            }
        };

        FinalTask()
        {
        }
    };

    template<typename T>
    auto finalize(ThreadPool* pool, T&& t) -> FinalTask<T>
    {
        co_return std::forward<T>(t);
    }

    auto finalize(ThreadPool* pool) -> FinalTask<void>
    {
        co_return;
    }

} // namespace rg
