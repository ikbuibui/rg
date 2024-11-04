#pragma once

#include "ThreadPool.hpp"

#include <coroutine>
#include <cstdint>
#include <iostream>

namespace rg
{
    // awaiter to place its own continuation to the pool stack and return to caller
    template<typename promise_type>
    struct DetachToPoolAwaiter
    {
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

            DetachToPoolAwaiter<promise_type> initial_suspend() noexcept
            {
                std::cout << "initial suspend called" << std::endl;

                return {};
            }

            std::suspend_always final_suspend() noexcept
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

        ~InitTask()
        {
            std::cout << "destroy rh" << std::endl;
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
