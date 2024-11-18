#pragma once

#include "ExecutionSpace.hpp"
#include "ThreadPool.hpp"

#include <cassert>
#include <cmath>
#include <coroutine>
#include <optional>
#include <utility>

namespace rg
{

    // TODO add promise type to coroutine handles
    struct exec_inplace_if
    {
        bool ready;

        constexpr bool await_ready() const noexcept
        {
            return ready;
        }

        template<typename TPromise>
        constexpr void await_suspend(std::coroutine_handle<TPromise> h) const noexcept
        {
            h.promise().pool_p->dispatch_task(h);
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

    // template<typename Callable, IsResourceAccess... Args>
    // struct DeferredCallable
    // {
    //     // Constructor to store the callable and its arguments
    //     DeferredCallable(Callable&& callable, Args&&... args)
    //         : callable_(std::forward<Callable>(callable))
    //         , args_(std::forward<Args>(args)...)
    //     {
    //     }

    //     // Invokes the callable with the stored arguments
    //     auto operator()()
    //     {
    //         return std::apply(
    //             [this](auto&&... args)
    //             {
    //                 return callable_(args.obj...); // Extract `obj` from each ResourceAccess
    //             },
    //             args_);
    //     }

    // private:
    //     Callable callable_;
    //     std::tuple<Args...> args_;
    // };


    // if(resourcesReady)
    //   return awaiter that suspends, adds continuation to stack, and executes task
    // elseif resources not ready
    //   already added handle to waiting task map/or set waiting atomic value, return awaiter that suspend never
    //   (executes the continuation)
    // TODO switch to IsResourceAccess
    template<typename THandle, IsResourceHandle... ResourceHandles>
    struct DispatchAwaiter
    {
        using ResourceAccessList
            = TypeList<ResourceAccess<ResourceHandles::resource_id, typename ResourceHandles::access_type>...>;

        bool resourcesReady = false;
        // takes ownership of the handle, and passes it on in await resume
        THandle taskHandleObj;

        DispatchAwaiter(THandle&& handle, ResourceHandles&&...) : taskHandleObj{std::forward<THandle>(handle)}
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
            // suspend only called when resources are ready
            // assert(resourcesReady);
            // emplace continuation to stack
            // TODO make sure the promise of the continuation can access the return type of
            std::cout << "push continuation onto stack" << std::endl;
            h.promise().pool_p->dispatch_task(h);

            // execute the coroutine
            return taskHandleObj.coro;
        }

        // passes the return object
        // TODO add return type
        auto await_resume() const noexcept
        {
            // return value
            return taskHandleObj;
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
        std::coroutine_handle<AwaitedPromise> coro;

        bool await_ready() const noexcept
        {
            // TODO check this. can done return true when result is not ready? while running code in initial suspend?
            return coro.done();
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
            std::lock_guard lock(coro.promise().mutex);
            // check again after lock is taken in case coro finished between await ready and locking in await suspend
            if(!coro.done())
            {
                // Safely set h as the getWaiterHandle for coro
                coro.promise().getWaiterHandle = h;

                // Suspend the coroutine
                return true;
            }
            return false;
        }

        // will only be called after the task is done
        auto await_resume() const noexcept
        {
            auto val = coro.promise().result.value();

            // TODO Add lock here so that childeren cant be done before destroy parent is set
            // think if it needs to lock with children done or deregister or both.
            // doesnt need to lock with final_suspend
            // children space is done
            if(coro.promise().space.done())
            {
                // deregister from parent already done by space, since task has finished and thats why we have resumed
                // the get
                // once task finished, it places the responsibility of deregistering to the space
                // destroy coroutine frame
                coro.destroy();
            }
            else
            {
                // destruction responsibilty is now with space
                coro.promise().space.destroyOnDone = true;
            }
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

    // wait counter methods
    // maybe add another one which does waitCounter += val, for resource registration
    // should this return void?
    template<typename promise_type>
    bool notify(std::coroutine_handle<promise_type> h)
    {
        auto counter = --h.promise().waitCounter;
        if(counter == 0)
            h.promise().pool->dispatch(h);
        return (counter == 0);
    }

    struct base_task_promise
    {
    };

    // parser coroutine return type
    // returns the value of the callable
    // I want to suspend_always initial_suspend it and then put its handle to the handle stack
    // handle stack will be eaten by the pool
    // TODO can i hold T as non optional, maybe if it is default constructible
    template<typename T>
    struct Task
    {
        struct promise_type
        {
            // TODO think should I hold this in task

            // initialized in await_transform of parent coroutine
            ThreadPool* pool_p{};
            // does this need to be optional?
            std::optional<T> result = std::nullopt;
            // needs to be atomic. multiple threads will change this if deregistering from resources together
            std::atomic<uint32_t> waitCounter = 0;
            // if .get is called and this coro is not done, add waiter handle here to notify on final suspend
            // someone else waits for the completion of this task.
            std::coroutine_handle<> getWaiterHandle = nullptr;
            // mutex to synchronize final suspend and .get() waiting which adds a dependency
            std::mutex mutex;

            // Task space for the children of this task. Passed in its ptr during await transform
            ExecutionSpace space{};
            ExecutionSpace* parentSpace_p{};

            // using ResourceIDs = typename decltype(callable)::ResourceIDTypeList;

            Task get_return_object()
            {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
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
            std::suspend_always final_suspend() noexcept
            {
                // Lock for final_suspend, get_awaiter, and execSpace remove. They cannot happen together
                std::lock_guard lock(mutex);

                auto coro = std::coroutine_handle<promise_type>::from_promise(*this);
                // if(children space is done)
                if(space.done())
                {
                    // deregister from parent
                    parentSpace_p->deregister(coro);

                    // child space is done and this task is also done;
                    // only returing the value from get is left.can be deregistered now;
                    // only destroy coro in get;
                }
                else
                {
                    // set flag SpaceHasToDeregister
                    space.deregisterOnDone = true;
                    space.parentSpace = parentSpace_p;
                    space.ownerHandle = coro;
                    // child space isnt done
                    // deal with deregister from parent in task space
                    // deal with destroy in get, when child task space is finished
                }

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
                return {};
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
            template<typename TDispatchAwaiter>
            auto await_transform(TDispatchAwaiter awaiter)
            {
                // Init
                auto coro = awaiter.handle.coro;
                // pass in the parent task space
                coro.promise().parentSpace_p = &space;
                // pass in the pool ptr
                coro.promise().pool_p = pool_p;

                coro.promise().space.pool_p = pool_p;
                // Init over

                // Register handle to all resources in the execution space
                auto wc = space.addDependencies(coro, awaiter);
                coro.promise().waitCounter = wc;
                std::cout << "value set in wait counter is " << wc << std::endl;
                // resources Ready based on reutrn value of register to resources or value of waitCounter
                bool resourcesReady = (wc == 0);

                // if(resourcesReady)
                //   return awaiter that suspends, adds continuation to stack, and executes task
                // elseif resources not ready
                //   task has been initialized with wait counter, waits for child notification to add to ready queue
                //   return awaiter that suspend never (executes the continuation)
                awaiter.resourcesReady = resourcesReady;
                return awaiter;
            }

            // TODO PASS BY REF? also in init
            template<typename U, typename AwaitedPromise>
            auto await_transform(GetAwaiter<U, AwaitedPromise> aw)
            {
                return aw;
            }
        };

        explicit Task(std::coroutine_handle<promise_type> h) : coro(h)
        {
        }

        ~Task()
        {
            if(coro)
            {
                // get hasnt been called, and we reach the destructor
                if(coro.done())
                {
                    if(coro.promise().space.done())
                    {
                        // deregister from parent already done by space, since task has finished and thats why we have
                        // resumed the get once task finished, it places the responsibility of deregistering to the
                        // space destroy coroutine frame
                        coro.destroy();
                    }
                    else
                    {
                        // destruction responsibilty is now with space
                        coro.promise().space.destroyOnDone = true;
                    }
                }
                else
                {
                    // destruction responsibilty is now with space
                    coro.promise().space.destroyOnDone = true;
                }
            }
            else
            {
                // get has already been called and has set coro to nullptr, do nothing
            }
        }

        bool resourcesReady() const
        {
            return coro.promise().resourcesReady;
        }

        // TODO put some of the on get destruction logic in destructor as well. If destroying the object without
        // calling get,
        auto get() -> GetAwaiter<T, promise_type>
        {
            auto awaiter = GetAwaiter<T, promise_type>{coro};
            coro = nullptr;
            // Set coro to nullptr. TODO get cannot be called twice
            // make sure coro isnt used again by the handle. Either destroyed or owned by the execution space
            return awaiter;
        }

        // TODO make this private:
        std::coroutine_handle<promise_type> coro;
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
    template<typename Callable, IsResourceHandle... ResourceHandles>
    auto dispatch_task(Callable&& callable, ResourceHandles&&... handles)
    {
        // TODO bind resources with restrictions applied
        // TODO Think about copies, references and lifetimes


        // create the awaitabletask coroutine.
        auto handle = std::invoke(std::forward<Callable>(callable), std::forward<ResourceHandles>(handles.obj)...);

        // auto bound_callable = std::bind_front(std::forward<Callable>(callable), handles.obj...);

        //  Bind the resources as arguments to the callable/coroutine
        // Deduce the return type of the callable with bound arguments
        // using ReturnType = decltype(callable(std::forward<Args>(res)...).get());


        // auto handle = callable(std::forward<Args>(res)...);

        // if(resourcesReady)
        //  return awaitable that puts the continuation on the stack, executes the task
        //  returns returnHandleObject
        // else
        //  return awaitable that puts the task on the st tack, resumes continuation on the stack
        //  returns task returnHandleObject


        // execute
        // x();
        //
        // final_suspend removes from task and notifies
        // return DispatchAwaiter{handle};
        //
        return DispatchAwaiter{handle, std::forward<ResourceHandles>(handles)...};
    }

    // TODO Dispatch for
    // - for short tasks, which dont need to place continuation to pool queue
    //   if ready, do and continue; else, place task in waiters
    // - for callables that are not rg::tasks, and dont need their own tasks space


} // namespace rg
