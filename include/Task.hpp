#pragma once

#include "FinalDelete.hpp"
#include "ResourceNode.hpp"
#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"
#include "dispatchTask.hpp"

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <optional>

namespace rg
{
    // When get is called on a handle return object
    // if coro is done - dont suspend - await ready true, and return promise value in await resume
    // if coro is not done - suspend (go to caller, if it is main, skip over everything, this will be resumed by the
    // pool when coro is done), add to waiter queue (worker will loop over this queue and check if done) Add promise
    // type for this awaiter
    // template on awaited promise to see if it holds a task space
    template<typename T, typename AwaitedPromise>
    struct GetAwaiter
    {
        // std::coroutine_handle<AwaitedPromise> coro;

        SharedCoroutineHandle coro;

        bool await_ready() const noexcept
        {
            // return coro.promise<AwaitedPromise>().task_done;
            // if it is 0, task is done, continue, if 1, then pause task
            return !coro.promise<AwaitedPromise>().workingState;
        }

        // has a lock to prevent final suspend of coro being done when await suspend is being called
        template<typename ContPromise>
        constexpr bool await_suspend(std::coroutine_handle<ContPromise> h) const noexcept
        {
            // once we aquire lock,
            // If coro not done, add h to its waiter handle and it will be done in final suspend
            // if coro is done, we can simply resume h
            // Moved outside to make lock small

            // We can move this lock into setHandle
            // std::lock_guard lock(coro.promise<AwaitedPromise>().mutex);
            // check again after lock is taken in case coro finished between await ready and locking in await suspend
            // if(!coro.promise<AwaitedPromise>().task_done)
            // {
            //     // Safely set h as the getWaiterHandle for coro
            //     coro.promise<AwaitedPromise>().getWaiterHandle = h;

            //     // Suspend the coroutine
            //     return true;
            // }
            // return false;
            coro.promise<AwaitedPromise>().getWaiterHandle = h;
            uint32_t expectedState = 1;
            coro.promise<AwaitedPromise>().workingState.compare_exchange_strong(expectedState, 2);
            // return true to suspend if expected state is 1
            return expectedState != 0;
        }

        // will only be called after the task is done
        auto await_resume() const noexcept
        {
            auto val = coro.promise<AwaitedPromise>().result.value();

            // TODO Add lock here so that childeren cant be done before destroy parent is set
            // think if it needs to lock with children done or deregister or both.
            // doesnt need to lock with final_suspend
            // children space is done
            // if(coro.promise().space.done())
            // {
            //     // deregister from parent already done by space, since task has finished and thats why we have
            //     resumed
            //     // the get
            //     // once task finished, it places the responsibility of deregistering to the space
            //     // destroy coroutine frame
            //     coro.destroy();
            // }
            // else
            // {
            //     // destruction responsibilty is now with space
            //     coro.promise().space.destroyOnDone = true;
            // }
            // TODO add locks to make sure when set to be destructible someone didnt deregister
            // if(coro.promise().space.empty())
            // {
            //     coro.destroy();
            // }
            // else
            // {
            //     coro.promise().space.setDestructible();
            // }

            return val;
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

        template<typename U>
        friend struct DispatchAwaiter;

        template<typename Callable, typename... ResourceAccess>
        friend auto dispatch_task(Callable&& callable, ResourceAccess&&... accessHandles);

        struct promise_type
        {
            // TODO think should I hold this in task

            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};
            // does this need to be optional?
            std::optional<T> result = std::nullopt;
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            // start from a large offset, add to it when registering
            // decrement the offset when registration is done to avoid races which start exec while registering
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            std::atomic<uint32_t> waitCounter{INVALID_WAIT_STATE};
            bool task_done = false;
            std::atomic<uint32_t> workingState = 1;
            // counts number of children alive
            // used for barrier
            std::atomic<uint32_t> childCounter{0u};
            std::atomic<uint32_t>* parentChildCounter{nullptr};
            // if .get is called and this coro is not done, add waiter handle here to notify on final suspend
            // someone else waits for the completion of this task.
            std::coroutine_handle<> getWaiterHandle{nullptr};

            // hold parent to keep it alive
            SharedCoroutineHandle parent;

            // hold self and reset in final suspend, helps to keep me alive even if returnObj is dead
            SharedCoroutineHandle self;

            // Task space for the children of this task. Passed in its ptr during await transform
            // std::shared_ptr<ExecutionSpace> space = std::make_shared<ExecutionSpace>();

            // hold res in vector to deregister later
            std::vector<std::shared_ptr<ResourceNode>> resourceNodes;

            // using ResourceIDs = typename decltype(callable)::ResourceIDTypeList;

            // template<typename... Args>
            // promise_type(Args...)
            //     : self{SharedCoroutineHandle(std::coroutine_handle<promise_type>::from_promise(*this))}
            // {
            // }
            ~promise_type()
            { // deregister from resources
                std::ranges::for_each(
                    resourceNodes,
                    [this](auto const& resNode)
                    { resNode->remove_task(std::coroutine_handle<promise_type>::from_promise(*this), pool_p); });

                // decrement child task counter of the parent
                if(--*parentChildCounter == 0)
                {
                    // if counter hits zero, notify parent cv
                    // handle.promise().cv.notify_all();
                }
            }

            Task get_return_object()
            {
                self = SharedCoroutineHandle(std::coroutine_handle<promise_type>::from_promise(*this));
                return Task{self};
            }

            // required to suspend as handle coroutine is created in dispactch task
            // waiter suspend. awaiter suspended for n resumes
            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            // do suspend_if, if returns void, suspend never (which will destroy the coroutine), if returns a value,
            // suspend_always and destroy in get/task space
            // Think about task space being done for deletion and suspension
            // if this doesnt suspend, then done will never return true
            // do suspend always? suspend if task space is not empty
            // if it returns something - try destroy after .get() or in the destructor of the handle object
            // it it returns nothing, try here
            // destroy cannot happen before final suspend
            FinalDelete final_suspend() noexcept
            {
                { // Lock for final_suspend, get_awaiter, and execSpace remove. They cannot happen together
                    // std::lock_guard lock(mutex);
                    uint32_t expectedState = 1;
                    workingState.compare_exchange_strong(expectedState, 0);

                    // TODO remove task done
                    task_done = true;
                    if(expectedState == 2) // get has been pushed handle already
                    {
                        // push getWaiterHandle to ready tasks;
                        pool_p->addTask(getWaiterHandle);

                        // when get is finally resumed, its await resume will take out the value, then we can try
                        // destruction if possible, else hand it over to the task space

                        // TODO think about optimization to resume instantly on this thread.
                        // Maybe if returning a coroutine in final suspend's await resume is possible, make the
                        // worker instantly execute it. But is the old coroutine destroyed already?
                    }
                }
                // we are done, no need to hold self anymore.
                // self.reset();
                // This is safe because at final suspend point no other threads will concurrently add siblings
                // all siblings have been added already
                // if(self.use_count() == 1)
                // {
                //     std::cout << "in if " << std::endl;
                // }
                // else
                // {
                //     std::cout << "in else " << std::endl;
                //     assert(self.use_count() != 0);
                //     // safe because this will not
                //     self.reset();
                // }
                return {std::move(self)};
            }

            void unhandled_exception()

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
            template<typename U>
            auto await_transform(DispatchAwaiter<U> awaiter)
            {
                // Init
                auto& awaiter_promise
                    = awaiter.handle.coro.template promise<typename decltype(awaiter.handle)::promise_type>();

                // pass in the parent task space
                // coro.promise().space->parentSpace = space;
                // pass in the pool ptr
                awaiter_promise.pool_p = pool_p;

                // coro.promise().space->pool_p = pool_p;
                // coro.promise().space->ownerHandle = coro.getHandle();
                awaiter_promise.parent = self;
                awaiter_promise.parentChildCounter = &childCounter;
                ++childCounter;

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

            // TODO PASS BY REF? also in init
            template<typename NonDispatchAwaiter>
            auto await_transform(NonDispatchAwaiter aw)
            {
                return aw;
            }
        };

        explicit Task(SharedCoroutineHandle const& h) : coro(h)
        {
        }

        Task() : coro()
        {
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
    };

    template<>
    struct Task<void>
    {
        template<typename TaskT>
        friend struct Task;

        template<typename U>
        friend struct InitTask;

        template<typename U>
        friend struct DispatchAwaiter;

        template<typename Callable, typename... ResourceAccess>
        friend auto dispatch_task(Callable&& callable, ResourceAccess&&... accessHandles);

        struct promise_type
        {
            // TODO think should I hold this in task

            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            // start from a large offset, add to it when registering
            // decrement the offset when registration is done to avoid races which start exec while registering
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            std::atomic<uint32_t> waitCounter{INVALID_WAIT_STATE};
            // counts number of children alive
            // used for barrier
            std::atomic<uint32_t> childCounter{0u};
            std::atomic<uint32_t>* parentChildCounter{nullptr};

            // hold parent to keep it alive
            SharedCoroutineHandle parent;

            // hold self and reset in final suspend, helps to keep me alive even if returnObj is dead
            SharedCoroutineHandle self;

            // Task space for the children of this task. Passed in its ptr during await transform
            // std::shared_ptr<ExecutionSpace> space = std::make_shared<ExecutionSpace>();

            // hold res in vector to deregister later
            std::vector<std::shared_ptr<ResourceNode>> resourceNodes;

            // using ResourceIDs = typename decltype(callable)::ResourceIDTypeList;

            // template<typename... Args>
            // promise_type(Args...)
            //     : self{SharedCoroutineHandle(std::coroutine_handle<promise_type>::from_promise(*this))}
            // {
            // }
            ~promise_type()
            { // deregister from resources
                std::ranges::for_each(
                    resourceNodes,
                    [this](auto const& resNode)
                    { resNode->remove_task(std::coroutine_handle<promise_type>::from_promise(*this), pool_p); });

                // decrement child task counter of the parent
                if(--*parentChildCounter == 0)
                {
                    // if counter hits zero, notify parent cv
                    // handle.promise().cv.notify_all();
                }
            }

            Task get_return_object()
            {
                self = SharedCoroutineHandle(std::coroutine_handle<promise_type>::from_promise(*this));
                return Task{self};
            }

            // required to suspend as handle coroutine is created in dispactch task
            // waiter suspend. awaiter suspended for n resumes
            std::suspend_always initial_suspend() noexcept
            {
                return {};
            }

            // do suspend_if, if returns void, suspend never (which will destroy the coroutine), if returns a value,
            // suspend_always and destroy in get/task space
            // Think about task space being done for deletion and suspension
            // if this doesnt suspend, then done will never return true
            // do suspend always? suspend if task space is not empty
            // if it returns something - try destroy after .get() or in the destructor of the handle object
            // it it returns nothing, try here
            // destroy cannot happen before final suspend
            FinalDelete final_suspend() noexcept
            {
                // we are done, no need to hold self anymore.
                // self.reset();
                // This is safe because at final suspend point no other threads will concurrently add siblings
                // all siblings have been added already
                // if(self.use_count() == 1)
                // {
                //     std::cout << "in if " << std::endl;
                // }
                // else
                // {
                //     std::cout << "in else " << std::endl;
                //     assert(self.use_count() != 0);
                //     // safe because this will not
                //     self.reset();
                // }
                return {std::move(self)};
            }

            void unhandled_exception()

            {
                std::terminate();
            }

            void return_void() {};

            // TODO contrain args to resource concept
            // TODO PASS BY REF? also in init
            // Called by children of this task
            // TODO think abour using a concept
            template<typename U>
            auto await_transform(DispatchAwaiter<U> awaiter)
            {
                // Init
                auto& awaiter_promise
                    = awaiter.handle.coro.template promise<typename decltype(awaiter.handle)::promise_type>();

                // pass in the parent task space
                // coro.promise().space->parentSpace = space;
                // pass in the pool ptr
                awaiter_promise.pool_p = pool_p;

                // coro.promise().space->pool_p = pool_p;
                // coro.promise().space->ownerHandle = coro.getHandle();
                awaiter_promise.parent = self;
                awaiter_promise.parentChildCounter = &childCounter;
                ++childCounter;

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

            // TODO PASS BY REF? also in init
            template<typename NonDispatchAwaiter>
            auto await_transform(NonDispatchAwaiter aw)
            {
                return aw;
            }
        };

        explicit Task(SharedCoroutineHandle const& h) : coro(h)
        {
        }

        Task() : coro()
        {
        }

    private:
        SharedCoroutineHandle coro;
    };
} // namespace rg
