#pragma once

#include "SharedCoroutineHandle.hpp"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <mutex>
#include <vector>

namespace rg
{
    struct BarrierQueue
    {
        using barrier_tuple
            = std::tuple<std::coroutine_handle<>, std::atomic<SharedCoroutineHandle::TRefCount>*, bool*>;

        std::vector<barrier_tuple> queue;
        mutable std::mutex mtx{};

        explicit BarrierQueue(std::size_t reserve_size = 4)
        {
            queue.reserve(reserve_size);
        }

        void emplace_back(SharedCoroutineHandle const& sharedHandle, bool& coroOutsideTask)
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.emplace_back(sharedHandle.get_coroutine_handle(), sharedHandle.use_count_ptr(), &coroOutsideTask);
        }

        // Iterate, remove elements with zero atomic value, and return their coroutine handles
        template<typename ThreadPool>
        void process_and_extract(ThreadPool* pool)
        {
            std::lock_guard<std::mutex> lock(mtx);

            // check if the barrier is ready
            // use count of 1, since parent task holds self. All children are done if use count - num handles = 1
            auto is_ready = [](barrier_tuple const& tuple) { return *std::get<1>(tuple) - *std::get<2>(tuple) == 1; };

            // Remove elements with zero atomic value and collect their coroutine handles
            auto it = queue.begin();
            while(it != queue.end())
            {
                if(is_ready(*it))
                {
                    pool->addTask(std::get<0>(*it));
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
