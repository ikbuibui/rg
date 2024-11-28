#pragma once

#include <coroutine>
#include <mutex>

struct BarrierAwaiter
{
    bool await_ready() noexcept
    {
        return false;
    };

    template<typename promise_type>
    void await_suspend(std::coroutine_handle<promise_type> h) noexcept
    {
        std::unique_lock<std::mutex> lock(h.promise().mtx);
        h.promise().cv.wait(lock, [this, h] { return h.promise().childCounter == 0; });
    };

    void await_resume() noexcept {};
};
