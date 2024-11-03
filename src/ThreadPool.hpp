#pragma once

#include <boost/lockfree/stack.hpp>

#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

// if size is known at compile time, get it from init as template param and use it for faster containers
constexpr uint32_t threadPoolStackSize = 64u;

// TODO SPECIFY PROMISE TYPE IN COROUTINE HANDLE
struct ThreadPool
{
    using stack_type = boost::lockfree::stack<std::coroutine_handle<>, boost::lockfree::capacity<threadPoolStackSize>>;

private:
    std::vector<std::jthread> threads;
    stack_type stack{};
    std::coroutine_handle<> finalize_handle;
    std::condition_variable cv;
    std::mutex mtx;

public:
    explicit ThreadPool(std::unsigned_integral auto size)
    {
        threads.reserve(size);
        std::generate_n(std::back_inserter(threads), size, [this] { return std::jthread(&ThreadPool::worker, this); });
    }

    // returns the return of the callable of the coroutine
    std::coroutine_handle<> emplace_init_frame(std::coroutine_handle<> h)
    {
        // lock to ensure the passed handle doesnt notify before cv wait is called wait till pool returns
        std::unique_lock<std::mutex> lock(mtx);
        stack.bounded_push(std::move(h));
        std::cout << "pushed onto the stack" << std::endl;
        // cv.wait(lock, is_empty());
        // removing predicate till real check is implemented, trivial check is optimized out
        cv.wait(lock);
        return finalize_handle;
    }

    void finalize(std::coroutine_handle<> finalize)
    {
        while(!is_empty())
        {
        }
        std::lock_guard<std::mutex> lock(mtx);
        finalize_handle = finalize;
        // there should only be one waiting
        cv.notify_one();
    }

private:
    // check if thread pool has no more work
    bool is_empty()
    {
        // if coro stack is empty and there is no more work
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return true;
    }

    void worker()
    {
        while(true)
        {
            // check for ready handles
            // steal others ready handles
            // if has ready handle
            //  - handle execute
            //  - remove from resources should be done inside handle execute
            //  - DO POOL WORK

            // check for uninitialized handles in globally ordered continuation stack
            // call uninit handle
            std::coroutine_handle<> h;
            if(stack.pop(h))
            {
                h.resume();
            }
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
    }
};
