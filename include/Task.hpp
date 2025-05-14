#pragma once

#include "CoroAllocator.hpp"
#include "FinalDelete.hpp"
#include "ResourceTaskQueue.hpp"
#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"
#include "dispatchTask.hpp"
#include "resources.hpp"

#include <atomic>
#include <coroutine>
#include <functional>
#include <utility>

namespace rg
{
    // When get is called on a handle return object
    // if coro is done - dont suspend - await ready true, and return promise value in await resume
    // if coro is not done - suspend (go to caller, if it is main, skip over everything, this will be resumed by the
    // pool when coro is done), add to waiter queue (worker will loop over this queue and check if done) Add promise
    // type for this awaiter
    // for void tasks this waits for task to complete
    // template on awaited promise to see if it holds a task space
    template<typename T, typename AwaitedPromise>
    struct GetAwaiter
    {
        // std::coroutine_handle<AwaitedPromise> coro;

        SharedCoroutineHandle coro;

        bool await_ready() const noexcept
        {
            // if it is 0, task is done, continue, if 1, then pause task
            return !coro.promise<AwaitedPromise>().workingState;
        }

        // has a lock to prevent final suspend of coro being done when await suspend is being called
        template<typename ContPromise>
        constexpr bool await_suspend(std::coroutine_handle<ContPromise> h) const noexcept
        {
            // If coro not done, add h to its waiter handle and it will be done in final suspend
            // if coro is done, we can simply resume h
            coro.promise<AwaitedPromise>().continuationHandle = h;
            uint32_t expectedState = 1;
            coro.promise<AwaitedPromise>().workingState.compare_exchange_strong(expectedState, 2);
            // return true to suspend if expected state is 1
            return expectedState != 0;
        }

        // will only be called after the task is done
        auto await_resume() const noexcept
        {
            auto result = std::move(coro.promise<AwaitedPromise>().result);
            coro.promise<AwaitedPromise>().coroOutsideTask = false;
            return result;
        }
    };

    // parser coroutine return type
    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    // TODO can i hold T as non optional, maybe if it is default constructible
    template<typename T>
    struct Task
    {
        friend struct Task<void>;

        template<typename U>
        friend struct InitTask;

        template<typename U, bool synchronous, bool finishedOnReturn>
        friend struct DispatchAwaiter;

        template<IsResource... TArgs>
        friend struct BarrierAwaiter;

        struct promise_type
        {
            using return_type = T;

            // needs to be atomic. multiple threads will change this if deregistering from resources together
            // start from a large offset, add to it when registering
            // decrement the offset when registration is done to avoid races which start exec while registering
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            alignas(hardware_destructive_interference_size) std::atomic<TWaitCount> waitCounter{INVALID_WAIT_STATE};
            // not incremented in constructor of shared handle
            alignas(hardware_destructive_interference_size)
                std::atomic<SharedCoroutineHandle::TRefCount> sharedOwnerCounter{1u};
            alignas(hardware_destructive_interference_size) std::atomic<uint32_t> workingState{1};

            // TODO think should I hold this in task
            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};

            // if .get is called and this coro is not done, add waiter handle here to notify on final suspend
            // someone else waits for the completion of this task.
            std::coroutine_handle<> continuationHandle{nullptr};

            // hold parent to keep it alive
            SharedCoroutineHandle parent;

            // hold self and reset in final suspend, helps to keep me alive even if returnObj is dead
            SharedCoroutineHandle self;

            // requires stable pointer access to taskData
            std::vector<std::pair<ResourceTaskQueue*, TaskData*>> resourceUsage;

            // does this need to be optional?
            T result;
            // true as the return object is always created
            bool coroOutsideTask = true;

            // using ResourceIDs = typename decltype(callable)::ResourceIDTypeList;

            // called with the copied in arguments
            // if a reference is passed, it a reference is copied to the coroutine state, and it can possibly dangle
            template<typename... Args>
            promise_type(ThreadPool* ptr, Args&... args)
                : pool_p{ptr}
                , self{SharedCoroutineHandle(
                      std::coroutine_handle<promise_type>::from_promise(*this),
                      sharedOwnerCounter)}
            {
                constexpr uint16_t resource_counter = (static_cast<uint16_t>(IsResourceAccess<Args>) + ... + 0);
                resourceUsage.reserve(resource_counter);
                waitCounter.fetch_add(resource_counter, std::memory_order_relaxed);
                // Register task to resources
                // Fold expression only for handles satisfying HasAccessType
                (...,
                 (
                     [this](auto& arg)
                     {
                         if constexpr(IsResourceAccess<decltype(arg)>)
                         {
                             resourceUsage.emplace_back(
                                 &arg.resource.getResNode().userQueue,
                                 arg.resource.getResNode().userQueue.add_task(
                                     {std::coroutine_handle<promise_type>::from_promise(*this),
                                      std::move(arg.moveAccessMode()),
                                      &waitCounter,
                                      pool_p}));
                         }
                     }(args)));
            }

            // workaround for lamdas which pass their implicit this parameter
            // not needed if we have C++23 static lambdas
            template<typename... Args>
            promise_type(auto&, ThreadPool* ptr, Args&... args) : promise_type(ptr, args...)
            {
            }

            Task get_return_object()
            {
                return Task{self};
            }

            // required to suspend as handle coroutine is created in dispactch task
            // waiter suspend. awaiter suspended for n resumes
            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            FinalDelete final_suspend() noexcept
            {
                // let go of parent
                parent.reset();
                // Deregister from resource queue
                for(auto& resUsage : resourceUsage)
                {
                    resUsage.first->remove_task(resUsage.second);
                }
                uint32_t expectedState = 1;
                workingState.compare_exchange_strong(expectedState, 0);
                // contHandle has been pushed already
                if(expectedState == 2)
                {
                    return {std::move(self), continuationHandle};
                    // when continuation is finally resumed, await_resume will take out the value
                }
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
            // TODO PASS BY REF? also in init
            // Called by children of this task
            // TODO think abour using a concept
            // TODO think about moving or passing by reference for awaiter
            template<typename U, bool synchronous, bool finishedOnReturn>
            auto& await_transform(DispatchAwaiter<U, synchronous, finishedOnReturn>& awaiter)
            {
                // Init
                auto& awaiter_promise
                    = awaiter.handle.coro.template promise<typename decltype(awaiter.handle)::promise_type>();

                if constexpr(!finishedOnReturn)
                {
                    awaiter_promise.parent = self;
                }

                // Init over

                // TODO added to the resources, and now peopple will try to remove old stuff and run this.
                // But I dont want to run this

                // resources Ready based on reutrn value of register to resources or value of waitCounter

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

            static void* operator new(std::size_t n)
            {
                return CoroAllocator::allocate(n).ptr;
            }

            static void operator delete(void* ptr, std::size_t n)
            {
                CoroAllocator::deallocate({ptr, n});
            }
        };

        explicit Task(SharedCoroutineHandle const& h) noexcept : coro(h)
        {
        }

        explicit Task() noexcept : coro()
        {
        }

        Task(Task const& x) = delete;

        Task(Task&& x) noexcept : coro{std::move(x.coro)}
        {
            x.isMoved = true;
        }

        Task& operator=(Task const& x) = delete;

        Task& operator=(Task&& x) noexcept
        {
            coro = std::move(x.coro);
            x.isMoved = true;
            return *this;
        }

        ~Task() noexcept
        {
            if(!isMoved && coro)
            {
                coro.promise<promise_type>().coroOutsideTask = false;
            }
        }

        // TODO put some of the on get destruction logic in destructor as well. If destroying the object without
        // calling get,
        auto get() -> GetAwaiter<T, promise_type>
        {
            // moved coro, calling get again is an error
            auto awaiter = GetAwaiter<T, promise_type>{std::move(coro)};
            // task holds self. Get called.
            // Set coro to nullptr.
            // TODO get cannot be called twice
            // make sure coro isnt used again by the handle. Either destroyed or owned by the execution space
            return awaiter;
        }

    private:
        SharedCoroutineHandle coro;
        bool isMoved = false;
    };

    struct TData;

    template<>
    struct Task<void>
    {
        template<typename TaskT>
        friend struct Task;

        template<typename U>
        friend struct InitTask;

        template<typename U, bool synchronous, bool finishedOnReturn>
        friend struct DispatchAwaiter;

        template<IsResource... TArgs>
        friend struct BarrierAwaiter;

        struct promise_type
        {
            using return_type = void;

            // needs to be atomic. multiple threads will change this if deregistering from resources together
            // start from a large offset, add to it when registering
            // decrement the offset when registration is done to avoid races which start exec while registering
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            alignas(hardware_destructive_interference_size) std::atomic<TWaitCount> waitCounter{INVALID_WAIT_STATE};
            // not incremented in constructor of shared handle
            alignas(hardware_destructive_interference_size)
                std::atomic<SharedCoroutineHandle::TRefCount> sharedOwnerCounter{1u};
            alignas(hardware_destructive_interference_size) std::atomic<uint32_t> workingState{1};

            // TODO think should I hold this in task
            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};
            // if .get is called and this coro is not done, add waiter handle here to notify on final suspend
            // someone else waits for the completion of this task.
            std::coroutine_handle<> continuationHandle{nullptr};

            // hold parent to keep it alive
            SharedCoroutineHandle parent;

            // hold self and reset in final suspend, helps to keep me alive even if returnObj is dead
            SharedCoroutineHandle self;

            // requires stable pointer access to taskData
            std::vector<std::pair<ResourceTaskQueue*, TaskData*>> resourceUsage;

            // true as the return object is always created
            bool coroOutsideTask = true;

            // using ResourceIDs = typename decltype(callable)::ResourceIDTypeList;

            // called with the copied in arguments
            // if a reference is passed, it a reference is copied to the coroutine state, and it can possibly dangle
            template<typename... Args>
            promise_type(ThreadPool* ptr, Args&... args)
                : pool_p{ptr}
                , self{SharedCoroutineHandle(
                      std::coroutine_handle<promise_type>::from_promise(*this),
                      sharedOwnerCounter)}
            {
                constexpr uint16_t resource_counter = (static_cast<uint16_t>(IsResourceAccess<Args>) + ... + 0);

                waitCounter.fetch_add(resource_counter, std::memory_order_relaxed);
                // Register task to resources
                // Fold expression only for ResourceHandles
                (...,
                 (
                     [this](auto& arg)
                     {
                         if constexpr(IsResourceAccess<decltype(arg)>)
                         {
                             resourceUsage.emplace_back(
                                 &arg.resource.getResNode().userQueue,
                                 arg.resource.getResNode().userQueue.add_task(
                                     {std::coroutine_handle<promise_type>::from_promise(*this),
                                      std::move(arg.moveAccessMode()),
                                      &waitCounter,
                                      pool_p}));
                         }
                     }(args)));
            }

            // workaround for lamdas which pass their implicit this parameter
            // not needed if we have C++23 static lambdas
            template<typename... Args>
            promise_type(auto&, ThreadPool* ptr, Args&... args) : promise_type(ptr, args...)
            {
            }

            Task get_return_object()
            {
                return Task{self};
            }

            // required to suspend as handle coroutine is created in dispactch task
            // waiter suspend. awaiter suspended for n resumes
            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            FinalDelete final_suspend() noexcept
            {
                parent.reset();
                // Deregister from resource queue
                for(auto& resUsage : resourceUsage)
                {
                    resUsage.first->remove_task(resUsage.second);
                }

                // get is never called, but void tasks may be called synchronously
                uint32_t expectedState = 1;
                workingState.compare_exchange_strong(expectedState, 0);
                // contHandle has been pushed already
                if(expectedState == 2)
                {
                    return {std::move(self), continuationHandle};
                }
                return {std::move(self)};
            }

            [[noreturn]] void unhandled_exception()
            {
                std::terminate();
            }

            void return_void()
            {
            }

            // TODO contrain args to resource concept
            // TODO PASS BY REF? also in init
            // Called by children of this task
            // TODO think abour using a concept
            // TODO think about moving or passing by reference for awaiter
            template<typename U, bool synchronous, bool finishedOnReturn>
            auto await_transform(DispatchAwaiter<U, synchronous, finishedOnReturn> awaiter)
            {
                // Init
                auto& awaiter_promise
                    = awaiter.handle.coro.template promise<typename decltype(awaiter.handle)::promise_type>();

                if constexpr(!finishedOnReturn)
                {
                    awaiter_promise.parent = self;
                }

                // Init over

                // TODO added to the resources, and now peopple will try to remove old stuff and run this.
                // But I dont want to run this

                // resources Ready based on reutrn value of register to resources or value of waitCounter

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

            static void* operator new(std::size_t n)
            {
                return CoroAllocator::allocate(n).ptr;
            }

            static void operator delete(void* ptr, std::size_t n)
            {
                CoroAllocator::deallocate({ptr, n});
            }
        };

        explicit Task(SharedCoroutineHandle const& h) noexcept : coro(h)
        {
        }

        explicit Task() noexcept : coro()
        {
        }

        Task(Task const& x) = delete;

        Task(Task&& x) noexcept : coro{std::move(x.coro)}
        {
            x.isMoved = true;
        }

        Task& operator=(Task const& x) = delete;

        Task& operator=(Task&& x) noexcept
        {
            coro = std::move(x.coro);
            x.isMoved = true;
            return *this;
        }

        ~Task() noexcept
        {
            if(!isMoved && coro)
            {
                coro.promise<promise_type>().coroOutsideTask = false;
            }
        }

    private:
        SharedCoroutineHandle coro;
        bool isMoved = false;
    };

    template<typename T>
    concept IsTask = traits::is_specialization_of_v<std::remove_cvref_t<T>, Task>;

    template<typename Callable, typename... Args>
    concept ReturnsTask = requires(Callable&& func, Args&&... args) {
        // Check that the callable can be invoked with the given arguments
        // and that its return type (after removing cv-ref qualifiers) is a specialization of rg::Task
        { std::invoke_result_t<Callable, Args...>() } -> IsTask;
    };

    template<bool synchronous = false, bool finishedOnReturn = false, typename... Args>
    auto dispatch_task(auto&& task, Args&&... args) requires ReturnsTask<decltype(task), Args...>
    {
        auto handle = std::invoke(std::forward<decltype(task)>(task), std::forward<Args>(args)...);

        return DispatchAwaiter<decltype(handle), synchronous, finishedOnReturn>{std::move(handle)};
    }

} // namespace rg
