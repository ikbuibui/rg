#pragma once

#include "Context.hpp"
#include "CoroAllocator.hpp"
#include "FinalDelete.hpp"
#include "ResourceTaskQueue.hpp"
#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"

#include <atomic>
#include <coroutine>
#include <functional>
#include <type_traits>
#include <utility>

namespace rg
{
    template<typename T, typename TPromise>
    struct Task;

    enum class DeleteEvent
    {
        Default,
        AlpakaEvent
    };

    template<typename T, DeleteEvent DelEvent = DeleteEvent::Default>
    struct task_promise
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

        T result;
        // true as the return object is always created
        bool coroOutsideTask = true;

        // using ResourceIDs = typename decltype(callable)::ResourceIDTypeList;

        // called with the copied in arguments
        // if a reference is passed, it a reference is copied to the coroutine state, and it can possibly
        // dangle
        template<typename... Args>
        task_promise(Context& ctx, Args&...)
            : pool_p{ctx.poolPtr}
            , parent{ctx.handleRef}
            , self{SharedCoroutineHandle(std::coroutine_handle<task_promise>::from_promise(*this), sharedOwnerCounter)}
        {
            ctx.handleRef = std::ref(self);
        }

        // workaround for lamdas which pass their implicit this parameter
        // not needed if we have C++23 static lambdas
        template<typename... Args>
        task_promise(auto&, Context& ctx, Args&... args) : task_promise(ctx, args...)
        {
        }

        Task<T, task_promise> get_return_object()
        {
            return Task<T, task_promise>{self};
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
            // To handle well maybe store std::exception_ptr exception_ptr_ = nullptr;
            // and here do exception_ptr_ = std::current_exception();
            std::terminate();
        }

        // allows conversions
        template<typename U>
        void return_value(U&& value) noexcept(
            std::is_nothrow_move_assignable_v<U> || std::is_nothrow_copy_assignable_v<U>)
            requires(!std::is_void_v<U> && std::is_convertible_v<std::decay_t<U>, T>)
        {
            result = std::forward<U>(value);
        }

        static void* operator new(std::size_t n)
        {
            return rg::CoroAllocator::allocate(n).ptr;
        }

        static void operator delete(void* ptr, std::size_t n)
        {
            rg::CoroAllocator::deallocate({ptr, n});
        }
    };

    template<DeleteEvent DelEvent>
    struct task_promise<void, DelEvent>
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
        // if a reference is passed, it a reference is copied to the coroutine state, and it can possibly
        // dangle
        template<typename... Args>
        task_promise(Context& ctx, Args&...)
            : pool_p{ctx.poolPtr}
            , parent{ctx.handleRef}
            , self{SharedCoroutineHandle(std::coroutine_handle<task_promise>::from_promise(*this), sharedOwnerCounter)}
        {
            ctx.handleRef = std::ref(self);
        }

        // workaround for lamdas which pass their implicit this parameter
        // not needed if we have C++23 static lambdas
        template<typename... Args>
        task_promise(auto&, Context& ctx, Args&... args) : task_promise(ctx, args...)
        {
        }

        Task<void, task_promise> get_return_object()
        {
            return Task<void, task_promise>{self};
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
            // To handle well maybe store std::exception_ptr exception_ptr_ = nullptr;
            // and here do exception_ptr_ = std::current_exception();
            std::terminate();
        }

        void return_void() noexcept
        {
        }

        static void* operator new(std::size_t n)
        {
            return rg::CoroAllocator::allocate(n).ptr;
        }

        static void operator delete(void* ptr, std::size_t n)
        {
            rg::CoroAllocator::deallocate({ptr, n});
        }
    };
} // namespace rg
