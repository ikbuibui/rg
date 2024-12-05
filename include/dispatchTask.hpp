#pragma once

#include "ResourceNode.hpp"
#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"

#include <cassert>
#include <cmath>
#include <condition_variable>
#include <coroutine>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace rg
{
    // Sentinel for the invalid waiter counter state during registration
    static constexpr uint32_t INVALID_WAIT_STATE = 1u << 31;

    struct DeleteAwaiter
    {
        SharedCoroutineHandle self;

        constexpr bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<>) noexcept
        {
            std::cout << "reset handle with use count : " << self.use_count() << std::endl;
            self.reset();
        }

        constexpr void await_resume() const noexcept
        {
        }
    };

    // Used as final suspend awaitable, as if there is a value to be returned, this will suspend the coroutine
    // thus keeping its frame alive, else dont suspend, which will destroy the coroutine
    struct suspend_if
    {
        bool condition;

        constexpr bool await_ready() const noexcept
        {
            return !condition;
        }

        constexpr void await_suspend(std::coroutine_handle<>) const noexcept
        {
        }

        constexpr void await_resume() const noexcept
        {
        }
    };


    // forward declaration
    template<typename T>
    struct Task;

    // if(resourcesReady)
    //   return awaiter that suspends, adds continuation to stack, and executes task
    // elseif resources not ready
    //   already added handle to waiting task map/or set waiting atomic value, return awaiter that suspend never
    //   (executes the continuation)
    // TODO switch to IsResourceAccess
    template<typename T>
    struct DispatchAwaiter
    {
        // using ResourceAccessList
        //     = TypeList<ResourceAccess<ResourceHandles::resource_id, typename ResourceHandles::access_type>...>;
        // takes ownership of the handle, and passes it on in await resume
        Task<T> handle;
        bool resourcesReady;

        DispatchAwaiter(Task<T>&& handleObj, bool resourcesReady)
            : handle{std::forward<Task<T>>(handleObj)}
            , resourcesReady{resourcesReady}
        {
        }

        bool await_ready() const noexcept
        {
            // suspend if resources ready, carry on continuation without suspend if not ready
            // suspend is false, dont suspend is true
            return !resourcesReady;
        }

        template<typename TPromise>
        std::coroutine_handle<> await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            // save here, as after dispatching self to the threadpool, this awaiter object (holding handle) may be
            // destroyed, then the return statement would be use after free
            auto resume_ready_handle = handle.coro.get_coroutine_handle();
            auto pool_p = h.promise().pool_p;
            // suspend only called when resources are ready
            // assert(resourcesReady);
            // emplace continuation to stack
            // TODO make sure the promise of the continuation can access the return type of
            pool_p->dispatch_task(h);

            // execute the coroutine
            // USING THIS is dangerous cont may be finished and destroy this awitable object
            return resume_ready_handle;
        }

        // template<typename TPromise>
        // bool await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        // {
        //     // save here, as after dispatching self to the threadpool, this awaiter object (holding handle) may be
        //     // destroyed, then the return statement would be use after free
        //     auto resume_ready_handle = handle.coro.get_coroutine_handle();
        //     // suspend only called when resources are ready
        //     // assert(resourcesReady);
        //     // emplace continuation to stack
        //     // TODO make sure the promise of the continuation can access the return type of
        //     h.promise().pool_p->addReadyTask(resume_ready_handle);

        //     // execute the coroutine
        //     // USING THIS is dangerous cont may be finished and destroy this awitable object
        //     return false;
        // }

        // passes the return object
        // TODO add return type
        auto await_resume() const noexcept
        {
            // return value
            return handle;
        }
    };

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
            return coro.promise<AwaitedPromise>().task_done;
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
            std::lock_guard lock(coro.promise<AwaitedPromise>().mutex);
            // check again after lock is taken in case coro finished between await ready and locking in await suspend
            if(!coro.promise<AwaitedPromise>().task_done)
            {
                // Safely set h as the getWaiterHandle for coro
                coro.promise<AwaitedPromise>().getWaiterHandle = h;

                // Suspend the coroutine
                return true;
            }
            return false;
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

    // forward declaration to make friend
    template<typename Callable, typename... ResourceAccess>
    auto dispatch_task(Callable&& callable, ResourceAccess&&... accessHandles);

    // parser coroutine return type
    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    // TODO can i hold T as non optional, maybe if it is default constructible
    template<typename T>
    struct Task
    {
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

            // mutex to synchronize final suspend and .get() waiting which adds a dependency
            // used for barrier
            std::mutex mutex;
            // used for barrier
            std::condition_variable cv;

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
            DeleteAwaiter final_suspend() noexcept
            {
                { // Lock for final_suspend, get_awaiter, and execSpace remove. They cannot happen together
                    std::lock_guard lock(mutex);

                    task_done = true;
                    if(getWaiterHandle) // get has been called already
                    {
                        // push getWaiterHandle to ready tasks;
                        pool_p->addReadyTask(getWaiterHandle);

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

    // is called with co_await and it thus gives access to awaitable to the calling coro
    // awaitable should store a future and its promise should be set when the coroutine handle is finished
    // executing when is the awaitable destroyed, where to hold the memory of the promise template<typename T>
    // callable must be coroutine returning task type
    // args must be resources
    //
    // TODO pass the pool to the task
    // TODO pass in a way that i dont have to store stuff and make copies
    // Callable is a Task
    // thread_local static uint counter = 0;

    // Helper function to process only handles satisfying HasAccessType
    template<typename Func, typename... AccessHandles>
    void for_each_access_type(Func&& func, AccessHandles&&... accessHandles)
    {
        // Expand the parameter pack and apply the function only to handles satisfying HasAccessType
        (...,
         (void) (HasAccessType<std::decay_t<AccessHandles>> ? func(std::forward<AccessHandles>(accessHandles))
                                                            : void()));
    }

    template<typename Callable, typename... ResourceAccess>
    auto dispatch_task(Callable&& callable, ResourceAccess&&... accessHandles)
    {
        // TODO bind resources with restrictions applied
        // TODO Think about copies, references and lifetimes
        // std::cout << "Counter for thread " << std::this_thread::get_id() << " is " << counter++ << std::endl;


        // Helper lambda to process each accessHandle
        auto process_handle = [](auto&& handle) -> decltype(auto)
        {
            if constexpr(HasAccessType<std::decay_t<decltype(handle)>>)
            {
                return handle.get(); // Call get() for types with access type
            }
            else
            {
                return std::forward<decltype(handle)>(handle); // Pass other types directly
            }
        };

        // create the awaitabletask coroutine.
        auto handle = std::invoke(
            std::forward<Callable>(callable),
            process_handle(std::forward<ResourceAccess>(accessHandles))...);
        // , std::forward<typename ResourceAccess::value_type>(accessHandles.get())...);

        auto& handlePromise = handle.coro.template promise<typename decltype(handle)::promise_type>();
        auto& resourceNodes = handlePromise.resourceNodes;
        // can access coro because it this function is a friend
        // TODO fix! this reserves too large a space, not all accessHandles are resources
        resourceNodes.reserve(sizeof...(accessHandles));

        // Fold expression only for handles satisfying HasAccessType
        (...,
         (
             [&resourceNodes, &handle, &handlePromise](auto const& accessHandle)
             {
                 if constexpr(HasAccessType<std::decay_t<decltype(accessHandle)>>)
                 {
                     resourceNodes.push_back(accessHandle.getUserQueue());

                     accessHandle.getUserQueue()->add_task(
                         {handle.coro.get_coroutine_handle(),
                          typename std::decay_t<decltype(accessHandle)>::access_type{},
                          &handlePromise.waitCounter});
                 }
             }(std::forward<ResourceAccess>(accessHandles))));

        // TODO make sure all resources are registered before someone deregistering sends this to readyQueue
        // for_each_access_type(
        //     [&handlePromise, &resourceNodes, &handle](auto const& accessHandle)
        //     {
        //         resourceNodes.push_back(accessHandle.getUserQueue());

        //         accessHandle.getUserQueue()->add_task(
        //             {handle.coro.get_coroutine_handle(),
        //              typename std::decay_t<decltype(accessHandle)>::access_type{},
        //              &handlePromise.waitCounter});
        //     },
        //     accessHandles...);


        auto wc = handlePromise.waitCounter.fetch_sub(INVALID_WAIT_STATE);
        bool resReady = (wc == INVALID_WAIT_STATE);

        // bool resReady = (handlePromise.waitCounter.fetch_sub(INVALID_WAIT_STATE) == INVALID_WAIT_STATE);

        std::cout << "wc " << wc << " resources ready?" << resReady << std::endl;
        // auto bound_callable = std::bind_front(std::forward<Callable>(callable), handles.obj...);

        //  Bind the resources as arguments to the callable/coroutine
        // Deduce the return type of the callable with bound arguments
        // using ReturnType = decltype(callable(std::forward<Args>(res)...).get());

        // if(resourcesReady)
        //  return awaitable that puts the continuation on the stack, executes the task
        //  returns returnHandleObject
        // else
        //  return awaitable that puts the task on the st tack, resumes continuation on the stack
        //  returns task returnHandleObject

        // final_suspend removes from task and notifies
        return DispatchAwaiter{std::move(handle), resReady};
    }

    // TODO Dispatch for
    // - for short tasks, which dont need to place continuation to pool queue
    //   if ready, do and continue; else, place task in waiters
    // - for callables that are not rg::tasks, and dont need their own tasks space


} // namespace rg
