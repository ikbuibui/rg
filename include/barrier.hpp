#pragma once

#include <atomic>
#include <coroutine>
#include <mutex>
#include <utility>
#include <vector>

namespace rg
{
    struct BarrierAwaiter
    {
        bool await_ready() noexcept
        {
            return false;
        };

        template<typename promise_type>
        bool await_suspend(std::coroutine_handle<promise_type> h) noexcept
        {
            // std::cout << "in barrier" << std::endl;

            h.promise().pool_p->barrier_queue.emplace_back(h, &h.promise().childCounter);

            return true;
            // auto backoff_time = std::chrono::microseconds(1); // Initial backoff time
            // auto const max_backoff_time = std::chrono::milliseconds(10); // Maximum backoff time

            // while(h.promise().childCounter.load(std::memory_order_acquire) != 0)
            // {
            //     if(backoff_time < max_backoff_time)
            //     {
            //         std::this_thread::sleep_for(backoff_time);
            //         backoff_time *= 2;
            //     }
            //     else
            //     {
            //         std::this_thread::yield();
            //     }
            // }
            // while(h.promise().childCounter != 0)
            // {
            // }

            // std::unique_lock lock(h.promise().mtx);
            // h.promise().cv.wait(lock, [this, h] { return h.promise().childCounter == 0; });
            // std::cout << "barrier done" << std::endl;
            // return false;
        };

        void await_resume() noexcept {};
    };

    struct BarrierQueue
    {
        using barrier_pair = std::pair<std::coroutine_handle<>, std::atomic<uint32_t>*>;

        std::vector<barrier_pair> queue;
        mutable std::mutex mtx;

        explicit BarrierQueue(std::size_t reserve_size = 4)
        {
            queue.reserve(reserve_size);
        }

        void emplace_back(std::coroutine_handle<> handle, std::atomic<uint32_t>* atomic_ptr)
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.emplace_back(handle, atomic_ptr);
        }

        // Iterate, remove elements with zero atomic value, and return their coroutine handles
        template<typename ThreadPool>
        void process_and_extract(ThreadPool* pool)
        {
            std::lock_guard<std::mutex> lock(mtx);

            // check if the barrier is ready
            auto is_ready = [](barrier_pair const& pair) { return pair.second->load() == 0; };

            // Remove elements with zero atomic value and collect their coroutine handles
            auto it = queue.begin();
            while(it != queue.end())
            {
                if(is_ready(*it))
                {
                    pool->addTask(it->first);
                    it = queue.erase(it); // Remove from queue
                }
                else
                {
                    ++it; // Move to the next element
                }
            }
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.empty();
        }

        std::size_t size() const
        {
            std::lock_guard<std::mutex> lock(mtx);
            return queue.size();
        }
    };

} // namespace rg
