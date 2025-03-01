#pragma once

#include "FinalDelete.hpp"
#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"
#include "dispatchTask.hpp"
#include "waitCounter.hpp"

#include <condition_variable>
#include <coroutine>
#include <mutex>
#include <optional>
#include <utility>

namespace rg
{
    // awaiter to place its own continuation to the pool stack and return to caller
    struct DetachToPoolAwaiter
    {
        bool await_ready() noexcept
        {
            return false;
        }

        template<typename promise_type>
        void await_suspend(std::coroutine_handle<promise_type> h) noexcept
        {
            // add me to stack
            // and return to main
            h.promise().pool_p->addTask(h);
        }

        void await_resume() noexcept
        {
        }
    };

    // parser coroutine return type

    // returns the value of the callable
    // I want to the awaiter of initial_suspend to put its handle to the handle stack
    // handle stack will be eaten by the pool
    // promise holds the pool ptr
    template<typename T>
    struct InitTask
    {
        struct promise_type
        {
            using return_type = T;

            // not incremented in constructor of shared handle
            alignas(hardware_destructive_interference_size)
                std::atomic<SharedCoroutineHandle::TRefCount> sharedOwnerCounter{1u};

            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};
            // does this need to be optional?
            std::optional<T> result = std::nullopt;
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            // std::atomic<TWaitCount> waitCounter = 0;
            // if .get is called and this coro is not done, add waiter handle here to notify on final suspend
            // someone else waits for the completion of this task.
            std::coroutine_handle<> continuationHandle = nullptr;
            // mutex to synchronize final suspend and .get() waiting which adds a dependency
            // used for barrier
            std::mutex mtx;
            // used for barrier
            std::condition_variable cv;
            SharedCoroutineHandle self;

            bool task_done = false;
            bool all_done = false;
            // true as the return object is always created
            bool coroOutsideTask = true;

            template<typename... Args>
            promise_type(ThreadPool* ptr, Args...) : pool_p{ptr}
            {
                // TODO can i merge this with init
                // rootSpace->pool_p = pool_p;
            }

            promise_type(promise_type const&) = delete;
            promise_type(promise_type&&) = delete;
            promise_type& operator=(promise_type const&) = delete;
            promise_type& operator=(promise_type&&) = delete;

            ~promise_type()
            {
                // std::cout << "all done" << std::endl;
                all_done = true;
                // std::lock_guard lock(mtx);
                // cv.notify_all();
            }

            InitTask get_return_object()
            {
                self = SharedCoroutineHandle(
                    std::coroutine_handle<promise_type>::from_promise(*this),
                    sharedOwnerCounter);
                return InitTask{self};
            }

            DetachToPoolAwaiter initial_suspend() noexcept
            {
                return {};
            }

            FinalDelete final_suspend() noexcept
            {
                // std::cout << "final suspend called" << std::endl;
                task_done = true;
                // rootSpace.reset();
                // notify thart work is finished here
                // finalize
                // std::lock_guard lock(mtx);
                // cv.notify_all();
                // self.reset();
                return {std::move(self)};
            }

            [[noreturn]] void unhandled_exception()
            {
                std::terminate();
            }

            // allows conversions
            template<typename U>
            void return_value(U&& value)
            {
                result = std::forward<U>(value);
            }

            // TODO contrain args to resource concept
            template<typename U, bool Synchronous, bool finishedOnReturn>
            auto& await_transform(DispatchAwaiter<U, Synchronous, finishedOnReturn>& awaiter)
            {
                // Init
                auto& awaiter_promise
                    = awaiter.handle.coro.template promise<typename decltype(awaiter.handle)::promise_type>();
                // pass in the parent task space
                // coro.promise().space->parentSpace = rootSpace;
                // pass in the pool ptr
                awaiter_promise.pool_p = pool_p;

                // coro.promise().space->ownerHandle = coro.getHandle();
                if constexpr(!finishedOnReturn)
                {
                    awaiter_promise.parent = self;
                }

                // Init over

                // if(resourcesReady)
                //   return awaiter that suspends, adds continuation to stack, and executes task
                // elseif resources not ready
                //   task has been initialized with wait counter, waits for child notification to add to ready queue
                //   return awaiter that suspend never (executes the continuation)
                return awaiter;
            }

            template<typename NonDispatchAwaiter>
            auto await_transform(NonDispatchAwaiter&& aw)
            {
                return std::forward<NonDispatchAwaiter>(aw);
            }
        };

        explicit InitTask(SharedCoroutineHandle const& h) noexcept : coro{h}
        {
        }

        InitTask(InitTask const& x) = delete;

        InitTask(InitTask&& x) = delete;

        InitTask& operator=(InitTask const& x) = delete;

        InitTask& operator=(InitTask&& x) = delete;

        ~InitTask() noexcept
        {
            if(coro)
            {
                // wait for task end, wait for root space to finish
                // if(coro.use_count() == 1)
                // {
                //     coro.reset();
                // }
                // if(coro.use_count() > 1)
                // {
                //     std::unique_lock lock(coro.promise<promise_type>().mtx);
                //     std::cout << "finalize locked. Task done : " << coro.promise<promise_type>().task_done
                //               << "child counter : " << coro.promise<promise_type>().childCounter << std::endl;
                //     coro.promise<promise_type>().cv.wait(
                //         lock,
                //         [this] {
                //             return coro.promise<promise_type>().task_done
                //                    && coro.promise<promise_type>().childCounter == 0;
                //         });
                // }
                // std::cout << "finalize called with use count : " << coro.use_count() << std::endl;
                //
                // auto backoff_time = std::chrono::microseconds(1); // Initial backoff time
                // auto const max_backoff_time = std::chrono::milliseconds(10); // Maximum backoff time

                // This handle holds one copy of coro
                while(*coro.use_count_ptr() > 1)
                {
                    // if(backoff_time < max_backoff_time)
                    // {
                    //     std::this_thread::sleep_for(backoff_time);
                    //     backoff_time *= 2;
                    // }
                    // else
                    {
                        std::this_thread::yield();
                    }
                }

                // while(coro.use_count() > 1)
                // {
                //     std::this_thread::sleep_for(std::chrono::seconds(3));
                //     // std::cout << "use count : " << coro.use_count() << std::endl;
                // }
                coro.promise<promise_type>().coroOutsideTask = false;
                coro.reset();

                // {
                //     std::unique_lock lock(coro.promise<promise_type>().mtx);
                //     coro.promise<promise_type>().cv.wait(
                //         lock,
                //         [this] { return coro.promise<promise_type>().childCounter == 0; });
                // }
                // {
                //     std::unique_lock lock(coro.promise().rootSpace->mtx);
                //     // maybe better to set a done bool in the execution space rather that checking empty resList
                //     coro.promise().rootSpace->cv.wait(lock, [this] { return coro.promise().rootSpace->done(); });
                // }
            }
        }

        std::optional<T> get()
        {
            // sleep till final suspend notifies
            // coro.done() is check for spurious wakeups
            // can also spin on coro.done, may be easier
            // while(!coro.done){} return coro.promise().result;
            if(!coro.promise<promise_type>().task_done)
            {
                std::unique_lock lock(coro.promise<promise_type>().mtx);
                coro.promise<promise_type>().cv.wait(lock, [this] { return coro.promise<promise_type>().task_done; });
                // todo make this exception safe
            }
            auto result = coro.promise<promise_type>().result.value();

            // coro.destroy();
            // make sure coro isnt used again by the handle. Either destroyed or owned by the execution space
            // coro = nullptr;
            return result;
        }

    private:
        SharedCoroutineHandle coro;
    };

    // TODO also make initTask and orchestrate for void

} // namespace rg
