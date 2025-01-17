#pragma once

// #include "MPMCQueue.hpp"
#include "BarrierQueue.hpp"
#include "dequeue.hpp"
#include "hwloc_ctx.hpp"
#include "random.hpp"

// #include <boost/lockfree/stack.hpp>
#include <hwloc.h>

#include <algorithm>
#include <concepts>
#include <coroutine>
#include <cstdint>
#include <optional>
#include <random>
#include <thread>
#include <vector>

namespace rg
{

    // if size is known at compile time, get it from init as template param and use it for faster containers
    constexpr uint32_t threadPoolStackSize = 64u;
    constexpr uint32_t randomStealAttempts = 64u;

    // TODO SPECIFY PROMISE TYPE IN COROUTINE HANDLE
    struct ThreadPool
    {
        // using stack_type
        //     = boost::lockfree::stack<std::coroutine_handle<>, boost::lockfree::capacity<threadPoolStackSize>>;
        // using stack_type = rigtorp::MPMCQueue<std::coroutine_handle<>>;
        using stack_type = riften::Deque<std::coroutine_handle<>>;

        template<typename... ResArgs>
        friend struct BarrierAwaiter;

    private:
        // bitfield where 0 is free and 1 is busy
        // std::atomic<uint64_t> worker_states{0};
        thread_local static inline stack_type* thread_queue_p;
        std::vector<std::unique_ptr<stack_type>> thread_queues;
        stack_type master_queue{threadPoolStackSize};
        BarrierQueue barrier_queue{};
        // stack_type stack{threadPoolStackSize};
        // stack_type readyQueue{threadPoolStackSize};
        std::stop_source stop_source;
        std::vector<std::jthread> threads;
        hwloc_topology_t const& topology;

    public:
        explicit ThreadPool(std::unsigned_integral auto size) : topology(HwlocTopology::getInstance())
        {
            int num_cores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
            if(num_cores < static_cast<int>(size))
            {
                throw std::runtime_error("Insufficient cores for thread pool size.");
            }

            // set thread queue p for main thread
            thread_queue_p = &master_queue;
            thread_queues.reserve(size);
            std::generate_n(
                std::back_inserter(thread_queues),
                size,
                [] { return std::make_unique<stack_type>(threadPoolStackSize); });

            threads.reserve(size);
            uint16_t i = 0;
            std::generate_n(
                std::back_inserter(threads),
                size,
                [this, &i, num_cores]
                {
                    return std::jthread(
                        [this, idx = i++, num_cores](std::stop_token stoken)
                        {
                            // Pin thread to core
                            ThreadPool::pinThreadToCore(idx % num_cores);
                            ThreadPool::worker(idx, stoken);
                        });
                }

            );
        }

        ~ThreadPool()
        {
            // std::cout << "pool destructor called" << std::endl;
            // while(!done())
            // {
            // }
            stop_source.request_stop();
            // Ensure all threads are joined and destroyed. Not needed if order of destruction is correct
            threads.clear();
            // threads.clear();
            // std::this_thread::sleep_for(std::chrono::seconds(3));
            // std::lock_guard<std::mutex> lock(mtx);
            // there should only be one waiting
            // cv.notify_one();
        }

        void addTask(std::coroutine_handle<> h)
        {
            thread_queue_p->emplace(h);
        }

        // void addReadyTask(std::coroutine_handle<> h)
        // {
        //     // std::cout << "added ready task" << std::endl;
        //     // readyQueue.bounded_push(h);
        //     readyQueue.push(h);
        // }

        // returns the return of the callable of the coroutine
        // void dispatch_task(std::coroutine_handle<> h)
        // {
        //     // std::cout << "added dispatch task" << std::endl;
        //     // stack.bounded_push(h);
        //     stack.push(h);
        // }

        void finalize() const
        {
            stop_source.request_stop();
        }

    private:
        // check if thread pool has no more work
        // bool done() const
        // {
        //     // TODO
        //     // if workers are free and coro stack is empty
        //     return worker_states == 0 && stack.empty() && readyQueue.empty();
        // }

        void pinThreadToCore(int core_index)
        {
            hwloc_obj_t core = hwloc_get_obj_by_type(topology, HWLOC_OBJ_CORE, core_index);
            if(!core)
            {
                throw std::runtime_error("Failed to get core for pinning.");
            }

            hwloc_cpuset_t cpuset = hwloc_bitmap_dup(core->cpuset);
            hwloc_bitmap_singlify(cpuset); // Restrict to a single core
            if(hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_THREAD) == -1)
            {
                hwloc_bitmap_free(cpuset);
                throw std::runtime_error("Failed to bind thread to core.");
            }
            hwloc_bitmap_free(cpuset);
        }

        void worker([[maybe_unused]] uint16_t index, std::stop_token stoken)
        {
            thread_queue_p = thread_queues[index].get();

            // mt19937 seems overkill. Heavier, higher quality random number
            // std::minstd_rand and XorShift are alternatives
            // https://github.com/ConorWilliams/Threadpool/blob/main/include/riften/xoroshiro128starstar.hpp
            thread_local rg::XorShift rng(std::random_device{}());
            std::uniform_int_distribution<uint32_t> dist(0, thread_queues.size() - 1);

            // uint64_t mask = (1ULL << index);
            // while(!done())
            // std::cout << "Thread created " << std::this_thread::get_id() << std::endl;
            // std::coroutine_handle<> h;
            std::optional<std::coroutine_handle<>> h;
            while(!stoken.stop_requested())
            {
                // if(thread_queues[index]->try_pop(h))
                // {
                //     h.resume();
                //     continue;
                // }
                h = thread_queues[index]->pop();
                if(h)
                {
                    h.value().resume();
                    continue;
                }

                // Attempt to steal from other queues
                // for(uint32_t i = 0; i < thread_queues.size(); ++i)
                // {
                //     if(i != index && thread_queues[i]->try_pop(h))
                //     {
                //         h.resume();
                //         break;
                //     }
                // }
                // increases latency when everyhting is done, as stealing may still be tried for a while after stop has
                // been requested
                // for(uint32_t attempts = 0; attempts < randomStealAttempts; ++attempts)
                // {
                //     size_t victim = dist(rng);
                //     if(victim != index && thread_queues[victim]->try_pop(h))
                //     {
                //         h.resume();
                //         break;
                //     }
                // }

                for(uint32_t attempts = 0; attempts < randomStealAttempts; ++attempts)
                {
                    size_t victim = dist(rng);
                    if(victim != index)
                    {
                        h = thread_queues[victim]->steal();
                        if(h)
                        {
                            break;
                        }
                    }
                    else
                    {
                        h = master_queue.steal();
                        if(h)
                        {
                            break;
                        }
                    }
                    // std::this_thread::yield();
                }

                if(h)
                {
                    h.value().resume();
                    continue;
                }

                // check barrier queue resumption
                barrier_queue.process_and_extract(this);
                // seperate this popping order into a function
                // if(readyQueue.try_pop(h))
                // {
                //     // std::cout << "ready popped" << std::endl;
                //     // worker_states.fetch_or(mask, std::memory_order_acquire); // Set worker as busy
                //     h.resume();
                //     // destruction of h is dealt with final suspend type or in get if something is returend
                //     // worker_states.fetch_and(~mask, std::memory_order_release); // Set worker as idle
                //     continue;
                // }
                // // TODO think about and fix race condition here. Pop happens but not marked busy
                // // if(stack.try_pop(h))
                // // {
                // //     // std::cout << "dispatch popped" << std::endl;
                // //     // worker_states.fetch_or(mask, std::memory_order_acquire); // Set worker as busy
                // //     h.resume();
                // //     // destruction of h is dealt with final suspend type or in get if something is returend
                // //     // worker_states.fetch_and(~mask, std::memory_order_release); // Set worker as idle
                // // }
            }
            // std::this_thread::yield();

            // check for ready handles
            // steal others ready handles
            // if has ready handle
            //  - handle execute
            //  - remove from resources should be done inside handle execute
            //  - DO POOL WORK

            // check for uninitialized handles in globally ordered continuation stack..
            // Currently coroutine frames which are tasks are malloc allocated, and have been registered with resources
            // once all resources are ready, coro handle will be pushed to ready queue
            // call uninit handle
            //  coros resume from initial suspend, start executing
            //  if they call co_await dispatch task inside the executable code,
            //      - register resources in dispatch task before initial suspend and await transform
            //          - let the coro handle be. When resources are ready it will be put in the ready queue
            //          - create awaiter with suspend_never derivative if resources are
            //          busy.
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
            //              - execute the code of dispatch_task
            //              - remove from resources in destructor of task promise
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
