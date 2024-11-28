#pragma once

#include <boost/lockfree/stack.hpp>

#include <concepts>
#include <coroutine>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace rg
{

    // if size is known at compile time, get it from init as template param and use it for faster containers
    constexpr uint32_t threadPoolStackSize = 64u;

    // TODO SPECIFY PROMISE TYPE IN COROUTINE HANDLE
    struct ThreadPool
    {
        using stack_type
            = boost::lockfree::stack<std::coroutine_handle<>, boost::lockfree::capacity<threadPoolStackSize>>;

    private:
        std::vector<std::jthread> threads;
        // bitfield where 0 is free and 1 is busy
        std::atomic<uint64_t> worker_states{0};
        stack_type stack{};
        stack_type readyQueue{};
        // std::condition_variable cv;
        // std::mutex mtx;
        std::stop_source stop_source;

    public:
        // counts the number of init tasks
        std::atomic<uint32_t> childCounter = 0;

        explicit ThreadPool(std::unsigned_integral auto size)
        {
            threads.reserve(size);
            uint16_t i = 0;
            std::generate_n(
                std::back_inserter(threads),
                size,
                [this, &i] { return std::jthread(&ThreadPool::worker, this, i++, stop_source.get_token()); });
        }

        ~ThreadPool()
        {
            std::cout << "pool destructor ccalled" << std::endl;
            // while(!done())
            // {
            // }
            stop_source.request_stop();

            // threads.clear();
            // std::this_thread::sleep_for(std::chrono::seconds(3));
            // std::lock_guard<std::mutex> lock(mtx);
            // there should only be one waiting
            // cv.notify_one();
        }

        void addReadyTask(std::coroutine_handle<> h)
        {
            std::cout << "added ready task" << std::endl;
            readyQueue.bounded_push(h);
        }

        // returns the return of the callable of the coroutine
        void dispatch_task(std::coroutine_handle<> h)
        {
            std::cout << "added dispatch task" << std::endl;
            stack.bounded_push(h);
        }

        void finalize()
        {
            stop_source.request_stop();
        }

    private:
        // check if thread pool has no more work
        bool done()
        {
            // TODO
            // if workers are free and coro stack is empty
            return worker_states.load(std::memory_order_acquire) == 0 && stack.empty() && readyQueue.empty();
        }

        void worker(uint16_t index, std::stop_token stoken)
        {
            // TODO FIX WORKER. Currently they go to sleep after init if tasks take time to be emplaced
            uint64_t mask = (1ULL << index);
            // while(!done())
            std::coroutine_handle<> h;
            while(true)
            {
                // seperate this popping order into a function
                if(readyQueue.pop(h))
                {
                    std::cout << "ready popped" << std::endl;
                    // worker_states.fetch_or(mask, std::memory_order_acquire); // Set worker as busy
                    h.resume();
                    // destruction of h is dealt with final suspend type or in get if something is returend
                    // worker_states.fetch_and(~mask, std::memory_order_release); // Set worker as idle
                    continue;
                }
                // TODO think about and fix race condition here. Pop happens but not marked busy
                if(stack.pop(h))
                {
                    std::cout << "dispatch popped" << std::endl;
                    // worker_states.fetch_or(mask, std::memory_order_acquire); // Set worker as busy
                    h.resume();
                    // destruction of h is dealt with final suspend type or in get if something is returend
                    // worker_states.fetch_and(~mask, std::memory_order_release); // Set worker as idle
                }
                if(stoken.stop_requested())
                {
                    return;
                }
            }
            // std::this_thread::yield();

            // check for ready handles
            // steal others ready handles
            // if has ready handle
            //  - handle execute
            //  - remove from resources should be done inside handle execute
            //  - DO POOL WORK

            // check for uninitialized handles in globally ordered continuation stack
            // call uninit handle
            //  coros resume from initial suspend, start executing
            //  if they call co_await emplace task inside the executable code,
            //      - register resources in initial suspend----- not await transform,
            //          - create awaiter with suspend_never derivative if resources are
            //          busy
            //              - create a coro with the emplace task code and add the
            //              handle to init handles with a counter which will be notified
            //              by resources it needes which are busy
            //              - when the emplace task code is finally executed, remove
            //              from resources
            //              - if it returns a void, destroy the coro mem
            //              - if it returns a value, stay alive and destroy when caller will call .get on the return
            //              object
            //              - continue parsing the code that called emplace task (BFS)
            //              - DO POOL WORK
            //          - create awaiter with suspend_always derivative if resources are
            //          free
            //              - add continuation to stack (DFS)
            //              - execute the code of emplace_task
            //              - remove from resources
            //              - DO POOL WORK
            //      - current continuation pauses
            //  else execute and DO POOL WORK
            // HOW TO SLEEP?
        }

    public:
        ThreadPool(ThreadPool const&) = delete;
        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool const&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;
    };
} // namespace rg
