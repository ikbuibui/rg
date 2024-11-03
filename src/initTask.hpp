#pragma once

#include "ThreadPool.hpp"

#include <coroutine>
#include <cstdint>
#include <iostream>

namespace rg
{
    // awaiter to place its own continuation to the pool stack
    template<typename promise_type>
    struct DetachToPoolAwaiter
    {
        // doesnt suspend the coroutine call
        bool await_ready() noexcept
        {
            return false;
        };

        std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept
        {
            // add me to stack
            // and suspend. Resumes finalize coroutine
            //  suspend with cv
            // wake and resume with finalize coroutine handle obtained from the pool
            std::cout << "handle to main cont emplaced" << std::endl;
            return h.promise().pool_p->emplace_init_frame(h);
        };

        void await_resume() noexcept {};
    };

    // parser coroutine return type

    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    // Holds the pool
    struct InitTask
    {
        struct promise_type
        {
            ThreadPool* pool_p;
            uint32_t size;

            promise_type(uint32_t size) : size{size}
            {
            }

            InitTask get_return_object()
            {
                return InitTask{std::coroutine_handle<promise_type>::from_promise(*this), size};
            }

            std::suspend_never initial_suspend() noexcept
            {
                return {};
            }

            DetachToPoolAwaiter<promise_type> final_suspend() noexcept
            {
                std::cout << "final suspend called" << std::endl;
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

        InitTask(std::coroutine_handle<promise_type> h, uint32_t size) : coro{h}, pool{size}
        {
            h.promise().pool_p = &pool;
            std::cout << "init return object created" << std::endl;
        }

        std::coroutine_handle<promise_type> coro;
        ThreadPool pool;

        ThreadPool* get()
        {
            return &pool;
        }
    };

    // hijack main and return its value from init
    // be explicit about type in exposed interface and auto deduce generically inside
    auto init(uint32_t size) -> InitTask
    {
        std::cout << "init coro running through" << std::endl;
        co_return;
    }
} // namespace rg
