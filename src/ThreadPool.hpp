#pragma once

#include <boost/lockfree/stack.hpp>

#include <concepts>
#include <condition_variable>
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
        std::coroutine_handle<> finalize_handle;
        std::condition_variable cv;
        std::mutex mtx;
        std::stop_source stop_source;

    public:
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
            while(!done())
            {
            }
            std::cout << "TP destructor called" << std::endl;
            stop_source.request_stop();

            // threads.clear();
            // std::this_thread::sleep_for(std::chrono::seconds(3));
            // std::lock_guard<std::mutex> lock(mtx);
            // there should only be one waiting
            // cv.notify_one();
        }

        void addReadyTask(std::coroutine_handle<> h)
        {
            std::cout << "pushing to ready tasks" << std::endl;
            readyQueue.bounded_push(h);
        }

        // returns the return of the callable of the coroutine
        void dispatch_task(std::coroutine_handle<> h)
        {
            stack.bounded_push(h);
            std::cout << "push done" << std::endl;
        }

        // returns the return of the callable of the coroutine
        void emplace_init_frame(std::coroutine_handle<> h)
        {
            // lock to ensure the passed handle doesnt notify before cv wait is called wait till pool returns
            // std::unique_lock<std::mutex> lock(mtx);
            stack.bounded_push(h);
            std::cout << "pushed onto the stack" << std::endl;
            // cv.wait(lock, done());
            // removing predicate till real check is implemented, trivial check is optimized out
            // dont go back to main until this is done
            // cv.wait(lock);
            // return finalize_handle;
        }

        void finalize(std::coroutine_handle<> finalize)
        {
            while(!done())
            {
            }
            std::lock_guard<std::mutex> lock(mtx);
            finalize_handle = finalize;
            // there should only be one waiting
            cv.notify_one();
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
            std::cout << "worker started on index " << index << " thread id " << std::this_thread::get_id()
                      << std::endl;
            // while(!done())
            while(true)
            {
                // seperate this popping order into a function
                std::coroutine_handle<> h;
                if(readyQueue.pop(h))
                {
                    std::cout << "ready queue popped " << std::this_thread::get_id() << std::endl;

                    worker_states.fetch_or(mask, std::memory_order_acquire); // Set worker as busy
                    h.resume();
                    // destruction of h is dealt with final suspend type or in get if something is returend
                    worker_states.fetch_and(~mask, std::memory_order_release); // Set worker as idle
                    continue;
                }
                // TODO think about and fix race condition here. Pop happens but not marked busy
                if(stack.pop(h))
                {
                    std::cout << "cont stack popped " << std::this_thread::get_id() << std::endl;

                    worker_states.fetch_or(mask, std::memory_order_acquire); // Set worker as busy
                    h.resume();
                    // destruction of h is dealt with final suspend type or in get if something is returend
                    worker_states.fetch_and(~mask, std::memory_order_release); // Set worker as idle
                }
                if(stoken.stop_requested())
                {
                    std::cout << "worker is requested to stop";
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
    };
} // namespace rg
