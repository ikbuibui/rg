
#pragma once

#include "ResourceAccess.hpp"
#include "ThreadPool.hpp"
#include "waitCounter.hpp"

#include <array>
#include <atomic>
#include <coroutine>
#include <cstdint>
#include <iostream>

namespace rg
{
    struct TaskData
    {
        // TODO deal with type erasure, need to call promise
        std::coroutine_handle<> handle; // Coroutine handle
        std::atomic<TWaitCount>* waitCounter_p{};
        ThreadPool* pool_ptr{nullptr};
        // uninitialized for a default constructed
        AccessMode accessMode{AccessMode::Uninitialized};
        // remove state 0 - default
        // remove state 1 - removed
        bool remove_state = 0;

        // TODO try passing T as parameter and then constructing
        template<typename TAccess>
        TaskData(
            std::coroutine_handle<> coro_handle,
            TAccess&& mode,
            std::atomic<uint32_t>* waitCtr_p,
            ThreadPool* pool_p)
            : handle(coro_handle)
            , waitCounter_p{waitCtr_p}
            , pool_ptr(pool_p)
            , accessMode(std::forward<TAccess>(mode))
        {
        }

        // TODO think about default access mode and waitPtr
        TaskData() : handle(nullptr)
        {
        }

        TaskData(TaskData const&) = delete;
        TaskData(TaskData&&) = default;
        TaskData& operator=(TaskData const&) = delete;
        TaskData& operator=(TaskData&&) = default;
        ~TaskData() = default;
    };

    // ResourceTaskQueue struct with firstNotReady and notify function
    // Doesnt do any bounds checking
    struct ResourceTaskQueue
    {
    private:
        alignas(hardware_destructive_interference_size) std::atomic<uint32_t> first = 0; // Iterator to the first task
        alignas(hardware_destructive_interference_size) std::atomic<uint32_t> firstNotReady
            = 0; // Iterator to the first not-ready task
        alignas(hardware_destructive_interference_size) std::atomic<uint32_t> last
            = 0; // Iterator to one past the last task
        // TODO replace with deque for stable iterators
        std::array<TaskData, 1024> tasks{};

    public:
        // Add a task to the list, incremenets wait counter of task if task is not immidiately ready to run
        // TODO think about rvalue reference
        // TODO think about returning something useful. Maybe bool telling if it is ready
        // returns a pointer to the
        // Constraints: Assumes that add_task can only be called by one thread at a time
        // Usage note: the wait counter of the handle add task is called on must be incremented before calling this
        //             function
        TaskData* add_task(TaskData&& task)
        {
            // simply add the task to the end
            auto old_last = last.load(std::memory_order_acquire);
            tasks[old_last] = std::move(task);
            // Not updating wait counter here as it is an optimizaiton to do it drectly in dispatch task
            // Publish updated last to other threads
            last.fetch_add(1, std::memory_order_release);

            // add only needs to set tasks to ready if FNR is last, i.e. no not ready tasks
            // if FNR is not old_last here then
            // either fnr is at an older task than old_last, and we cant be ready
            // or fnr is at new last, and we dont need to do anything (other threads can already see the new last here)
            auto fnr = firstNotReady.load(std::memory_order_acquire);
            if(fnr == old_last)
            {
                // we take responsibility to update
                // update if no one blocks
                // if empty or is parallel with previous task (prev task exists because not empty)
                // we rely on short circuiting the if condition
                // if first is already at new last, then remove must increment fnr as well
                if(first.load(std::memory_order_acquire) == old_last
                   || !is_serial_access(task.accessMode, tasks[old_last - 1].accessMode))
                {
                    if(firstNotReady.compare_exchange_strong(
                           old_last,
                           fnr + 1,
                           std::memory_order_acq_rel,
                           std::memory_order_relaxed))
                    {
                        task.waitCounter_p->fetch_sub(1, std::memory_order_relaxed);
                    }
                }
            }
            return &tasks[old_last];
        }

        // Removes all active tasks from the list which have a handle
        // Task is not guaranteed to be present in this ResourceTaskQueue, may not be registered here
        // TODO: maybe return some useful info
        // doesnt need to be a templated type, type erased handle is enough
        void remove_task(TaskData* tData)
        {
            // TODO ues pointer directly instead of search
            auto cur = first.load(std::memory_order_acquire);
            if(tasks[cur].handle == tData->handle)
            {
                // start to really remove tasks
                cur = first.fetch_add(1, std::memory_order_acq_rel) + 1;
                // delete the node

                bool found_completed_task_exit = false;

                // TODO is this while loop enforcing things correctly? I want the check for the remove state and
                // termination at fnr before the compare exchange is tried
                while((found_completed_task_exit = (tasks[cur].remove_state == 1))
                      && cur != firstNotReady.load(std::memory_order_acquire))
                {
                    // TODO is it possible to move this into while loop?
                    if(first.compare_exchange_strong(cur, ++cur, std::memory_order_acq_rel, std::memory_order_relaxed))
                    {
                        // delete the node
                        continue;
                    }
                    else
                    {
                        return;
                    }
                    if(!found_completed_task_exit)
                    {
                        return;
                    }
                }
            }
            else
            {
                // safe to increment cur since first cannot be fnr if remove is called
                ++cur;
                // safe to load current state, as if someone adds a task and changes FNR after this, it is not the task
                // which was removed.
                auto fnr = firstNotReady.load(std::memory_order_acquire);
                // post condition of while: cur != fnr since remove will definitely find the task before fnr
                //                          cur == fnr only possible if we took charge of updating
                // TODO remove while loop by also storing position in resource node in the task promise
                while(cur != fnr)
                {
                    if(tasks[cur].handle == tData->handle)
                    {
                        // delete the node
                        // publish as removed (Here publish state before checking first. While actually deleting we
                        // initially move and publish first and then check state)
                        tasks[cur].remove_state = 1;

                        if(first.compare_exchange_strong(
                               cur,
                               ++cur,
                               std::memory_order_acq_rel,
                               std::memory_order_relaxed))
                        {
                            // we have taken over upadting

                            bool found_completed_task_exit = false;

                            // cant use loaded fnr as we more ready tasks might be added and finished as we are
                            // removing stuff
                            while((found_completed_task_exit = (tasks[cur].remove_state == 1))
                                  && cur != firstNotReady.load(std::memory_order_acquire))
                            {
                                // TODO is it possible to move this into while loop?
                                if(first.compare_exchange_strong(
                                       cur,
                                       ++cur,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed))
                                {
                                    // delete the node
                                    continue;
                                }
                                else
                                {
                                    return;
                                }
                            }
                            if(!found_completed_task_exit)
                            {
                                return;
                            }
                        }
                        else
                        {
                            return;
                        }
                        break;
                    }
                    ++cur;
                }
            }

            // task_access has been modified or deleted
            // starting from the firstNotReady task, check if task can be set to ready
            // precondition : There are no already running tasks, i.e. firstNotReady is tasks.begin()
            // Assumes if a task is blocked all future tasks will be blocked. Not true for area resources
            // THINK ABOUT THIS PROPERTY. It holds for read write resources

            // start updating tasks
            auto fnr = firstNotReady.load(std::memory_order_acquire);
            auto temp_last = last.load(std::memory_order_acquire);
            AccessMode firstNewReadyMode{AccessMode::Uninitialized};
            // no more running tasks and there is atleast one not ready task
            // this task is next in line to run. Nothing is blocking it
            if(fnr == cur && temp_last != cur)
            {
                if(firstNotReady
                       .compare_exchange_strong(fnr, fnr + 1, std::memory_order_acq_rel, std::memory_order_relaxed))
                {
                    firstNewReadyMode = tasks[fnr].accessMode;
                    if(tasks[fnr].waitCounter_p->fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
                        // move handle to ready tasks queue
                        tData->pool_ptr->addTask(tasks[fnr].handle);
                    }
                    ++fnr;
                }
                else
                {
                    return;
                }
            }
            else
            {
                return;
            }
            bool parallel_exit = false;
            do
            {
                temp_last = last.load(std::memory_order_acquire);
                while(fnr != temp_last
                      && (parallel_exit = !is_serial_access(firstNewReadyMode, tasks[fnr].accessMode)))
                {
                    if(firstNotReady.compare_exchange_strong(
                           fnr,
                           fnr + 1,
                           std::memory_order_acq_rel,
                           std::memory_order_relaxed))
                    {
                        if(tasks[fnr].waitCounter_p->fetch_sub(1, std::memory_order_acq_rel) == 1)
                        {
                            if(!tData->pool_ptr)
                            {
                                std::cerr << "ResourceTaskQueue: No pool_ptr set" << std::endl;
                            }

                            // move handle to ready tasks queue
                            tData->pool_ptr->addTask(tasks[fnr].handle);
                        }
                        ++fnr;
                    }
                    else
                    {
                        return;
                    }
                }
                if(!parallel_exit)
                {
                    return;
                }
            } while(temp_last != last.load(std::memory_order_acquire));


            return;
        }
    };
} // namespace rg
