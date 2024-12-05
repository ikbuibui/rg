#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <iostream>
#include <mutex>
#include <thread>

struct BarrierAwaiter
{
    bool await_ready() noexcept
    {
        return false;
    };

    template<typename promise_type>
    bool await_suspend(std::coroutine_handle<promise_type> h) noexcept
    {
        std::cout << "in barrier" << std::endl;

        auto backoff_time = std::chrono::microseconds(1); // Initial backoff time
        auto const max_backoff_time = std::chrono::milliseconds(10); // Maximum backoff time

        while(h.promise().childCounter.load(std::memory_order_acquire) != 0)
        {
            if(backoff_time < max_backoff_time)
            {
                std::this_thread::sleep_for(backoff_time);
                backoff_time *= 2;
            }
            else
            {
                std::this_thread::yield();
            }
        }
        // while(h.promise().childCounter != 0)
        // {
        // }
        std::cout << "barrier done" << std::endl;
        // std::unique_lock<std::mutex> lock(h.promise().mtx);
        // h.promise().cv.wait(lock, [this, h] { return h.promise().childCounter == 0; });
        return false;
    };

    void await_resume() noexcept {};
};
