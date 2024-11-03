#pragma once

#include "ThreadPool.hpp"

#include <concepts>
#include <coroutine>
#include <cstdint>
#include <optional>
#include <utility>

// uint32_t size;

// void init(uint32_t _size)
// {
//     size = _size;
// }

// parser coroutine return type
// returns the value of the callable
// I want to suspend_always initial_suspend it and then put its handle to the handle stack
// handle stack will be eaten by the pool
template<typename T>
struct MainTask
{
    struct promise_type
    {
        std::optional<T> result;

        MainTask get_return_object()
        {
            return MainTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept
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

        auto await_transform()
        {
        }

        template<typename U>
        void return_value(U&& value)
        {
            result = std::forward<U>(value);
        }
    };

    std::coroutine_handle<promise_type> coro;

    explicit MainTask(std::coroutine_handle<promise_type> h) : coro(h)
    {
    }

    ~MainTask()
    {
        if(coro)
            coro.destroy();
    }

    MainTask(MainTask const&) = delete;
    MainTask(MainTask&&) = delete;
    MainTask& operator=(MainTask const&) = delete;
    MainTask& operator=(MainTask&&) = delete;

    T get()
    {
        return coro.promise().result.value();
    }
};

template<>
struct MainTask<void>
{
    struct promise_type
    {
        MainTask get_return_object()
        {
            return MainTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept
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

    std::coroutine_handle<promise_type> coro;

    explicit MainTask(std::coroutine_handle<promise_type> h) : coro(h)
    {
    }

    ~MainTask()
    {
        if(coro)
            coro.destroy();
    }

    MainTask(MainTask const&) = delete;
    MainTask(MainTask&&) = delete;
    MainTask& operator=(MainTask const&) = delete;
    MainTask& operator=(MainTask&&) = delete;

    void get()
    {
    }
};

// Coroutine function template that wraps the callable
template<typename Callable, typename... Args>
auto wrap_in_coroutine(Callable&& callable, Args&&... args)
    -> MainTask<decltype(callable(std::forward<Args>(args)...))>
{
    if constexpr(std::is_void_v<std::invoke_result_t<Callable, Args...>>)
    {
        callable(std::forward<Args>(args)...);
        co_return;
    }
    else
    {
        co_return callable(std::forward<Args>(args)...);
    }
}

// Function that accepts a callable, calls the coroutine, and returns the result
// handle stack will be eaten by the pool
// then i want this coroutine to go to sleep and wait to be notified when execution is completed and value is returned
template<typename Callable, typename... Args>
auto call_and_return_coroutine_result(Callable&& callable, Args&&... args)
{
    // create pool from object created by init
    // auto pool = ThreadPool{size};
    // auto task = wrap_in_coroutine(std::forward<Callable>(callable), std::forward<Args>(args)...);
    // return pool.emplace_init_frame(std::move(task.coro));
}

struct rg2
{
    rg2(std::unsigned_integral auto size) : pool{ThreadPool{size}}
    {
    }

    ~rg2()
    {
    }

private:
    ThreadPool pool;
};
