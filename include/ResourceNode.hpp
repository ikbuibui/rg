
#pragma once

#include "ThreadPool.hpp"
#include "resources.hpp"

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <mutex>
#include <variant>

namespace rg
{
    // Forward declaration of ResourceNode
    struct ResourceNode;

    struct task_access
    {
        // TODO deal with type erasure, need to call promise
        std::coroutine_handle<> handle; // Coroutine handle
        // std::reference_wrapper<std::type_info const> access_mode; // Type information
        std::atomic<uint32_t>* waitCounter_p{};
        AccessMode accessMode;

        // TODO try passing T as parameter and then constructing
        template<typename TAccess>
        task_access(std::coroutine_handle<> coro_handle, TAccess&& mode, std::atomic<uint32_t>* waitCtr_p)
            : handle(coro_handle)
            , waitCounter_p{waitCtr_p}
            , accessMode(std::forward<TAccess>(mode))
        {
        }

        // task_access(std::coroutine_handle<> coro_handle, std::type_info const& mode)
        //     : handle(coro_handle)
        //     , access_mode(mode)
        // {
        // }

        // TODO think about default access mode and waitPtr
        // task_access() : handle(nullptr)
        // {
        // }

        // task_access() : handle(nullptr), access_mode(typeid(void))
        // {
        // }
    };

    // ResourceNode struct with firstNotReady and notify function
    struct ResourceNode
    {
    private:
        uint32_t resource_uid; // Unique identifier for the resource
        std::forward_list<task_access> tasks; // List of task_access instances
        std::forward_list<task_access>::iterator firstNotReady; // Iterator to the first not-ready task
        std::forward_list<task_access>::iterator lastTask; // Iterator to the last task before end

        std::mutex mtx;

    public:
        // Constructor
        ResourceNode(uint32_t uid)
            : resource_uid(uid)
            , tasks{}
            , firstNotReady(tasks.end())
            , lastTask(tasks.before_begin())
        {
            // std::cout << "node id : " << resource_uid << " created" << std::endl;
        }

        auto getId() const
        {
            return resource_uid;
        }

        // Add a task to the list, incremenets wait counter of task if task is not immidiately ready to run
        // TODO think about rvalue reference
        // TODO think about returning somethin useful. Maybe bool telling if it is ready
        void add_task(task_access&& task)
        {
            // std::cout << "add task on node id : " << resource_uid << std::endl;

            // add task to resrource queue
            // TODO think about empalce
            // TODO think about locking and when to insert (maybe insert after checking for blocking).
            // 2 threads cant add together in this design
            // lastTask = tasks.insert_after(lastTask, task);
            // check ready
            std::lock_guard lock(mtx);

            if(firstNotReady == tasks.end())
            {
                // all previous tasks are ready
                // no tasks in queue
                // Add as ready
                if(firstNotReady == tasks.begin())
                {
                    // std::cout << "empty queue" << std::endl;
                    lastTask = tasks.insert_after(lastTask, task);
                    // set to end all the time since end might change after adding a task
                    firstNotReady = tasks.end();
                    return;
                }
                // check if some ready task blocks me
                if(is_serial_access(task.accessMode, tasks.cbegin()->accessMode))
                {
                    // someone is blocking, add and wait
                    // std::cout << "soemone is blocking" << std::endl;
                    lastTask = tasks.insert_after(lastTask, task);
                    firstNotReady = lastTask;
                    task.waitCounter_p->fetch_add(1, std::memory_order_relaxed);
                }
                else
                {
                    //  no one is blocking. Add as ready
                    // std::cout << "no one blocking" << std::endl;
                    lastTask = tasks.insert_after(lastTask, task);
                    // set to end all the time since end might change after adding a task
                    firstNotReady = tasks.end();
                }
            }
            else
            {
                // std::cout << "first not ready is not end" << std::endl;

                lastTask = tasks.insert_after(lastTask, task);
                // firstNotReady stays the same
                task.waitCounter_p->fetch_add(1, std::memory_order_relaxed);
                // there is a not ready task before us, so we cannot be ready. Add and wait
            }
        }

        // Removes all active tasks from the list which have a handle
        // Task is not guaranteed to be present in this ResourceNode, may not be registered here
        // returns bool : true if there are no more tasks in the list, signal to delete the node
        // doesnt need to be a templated type, type erased handle is enough
        bool remove_task(std::coroutine_handle<> handle, ThreadPool* pool_p)
        {
            // TODO use erase if or find if?
            // Since task was run, is done, and is being removed, it must have been in the running tasks
            // Assuming only one of a handle is added
            std::lock_guard lock(mtx);
            auto first = tasks.before_begin();
            auto next = std::next(first);

            // This should always find and remove a task
            while(next != firstNotReady)
            {
                // found handle here
                if(next->handle == handle)
                {
                    if(next == lastTask)
                    {
                        lastTask = first;
                    }
                    tasks.erase_after(first);
                    // erased after first.
                    // We have a new next. No need to increment first
                    update_ready(pool_p);
                }
                else
                {
                    ++first;
                }
                next = std::next(first);
            }
            return tasks.empty();
        }

        // TODO remove unused func
        // Check if the ResourceNode is empty
        bool empty() const
        {
            return tasks.empty(); // Returns true if the list is empty
        }


    private:
        // task_access has been modified or deleted
        // starting from the firstNotReady task, check if task can be set to ready
        // Assumes if a task is blocked all future tasks will be blocked. Not true for area resources
        // THINK ABOUT THIS PROPERTY. It holds for read write resources
        // TODO think of mutexes. Update, add,remove and checkBlocking
        // returns handle if it can be resumed,
        void update_ready(ThreadPool* pool_p)
        {
            auto first = tasks.begin();
            if(firstNotReady == tasks.end() || firstNotReady != first)
            {
                return;
            }
            // can only start seting tasks ready if firstNotReady reaches first position
            if(firstNotReady->waitCounter_p->fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                // move handle to ready tasks queue
                pool_p->addTask(firstNotReady->handle);
            }
            firstNotReady++;
            auto first_accessMode = first->accessMode;
            auto last = tasks.end();
            while(firstNotReady != last)
            {
                if(is_serial_access(firstNotReady->accessMode, first_accessMode))
                {
                    // Stop when a serial access is found
                    break;
                }
                else
                {
                    if(firstNotReady->waitCounter_p->fetch_sub(1, std::memory_order_acq_rel) == 1)
                    {
                        // move handle to ready tasks queue
                        pool_p->addTask(firstNotReady->handle);
                    }
                    firstNotReady++;
                }
            }
            return;
        }
    };
} // namespace rg
