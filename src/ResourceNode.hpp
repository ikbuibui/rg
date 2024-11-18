
#pragma once

#include "ThreadPool.hpp"
#include "resources.hpp"

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <variant>

namespace rg
{
    // Forward declaration of ResourceNode
    struct ResourceNode;

    struct task_access
    {
        using AccessModes = std::variant<access::read, access::write>;

        // TODO deal with type erasure, need to call promise
        std::coroutine_handle<> handle; // Coroutine handle
        // std::reference_wrapper<std::type_info const> access_mode; // Type information

        AccessModes accessMode;
        std::atomic<uint32_t>* waitCounter_p{};

        // TODO try passing T as parameter and then constructing
        template<typename TAccess>
        task_access(std::coroutine_handle<> coro_handle, TAccess mode, std::atomic<uint32_t>* waitCounter_p)
            : handle(coro_handle)
            , accessMode(mode)
            , waitCounter_p{waitCounter_p}
        {
        }

        // task_access(std::coroutine_handle<> coro_handle, std::type_info const& mode)
        //     : handle(coro_handle)
        //     , access_mode(mode)
        // {
        // }

        // TODO think about default access mode and waitPtr
        task_access() : handle(nullptr)
        {
        }

        // task_access() : handle(nullptr), access_mode(typeid(void))
        // {
        // }
    };

    // ResourceNode struct with firstNotReady and notify function
    struct ResourceNode
    {
        uint32_t resource_uid; // Unique identifier for the resource
        std::forward_list<task_access> tasks{}; // List of task_access instances
        std::forward_list<task_access>::iterator firstNotReady; // Iterator to the first not-ready task
        std::forward_list<task_access>::iterator lastTask; // Iterator to the last task before end

        // Constructor
        ResourceNode(uint32_t uid) : resource_uid(uid), firstNotReady(tasks.end()), lastTask(tasks.before_begin())
        {
        }

        bool isSerial(typename task_access::AccessModes x, typename task_access::AccessModes y)
        {
            return std::visit(
                [](auto const& lhs, auto const& rhs)
                {
                    return is_serial_access(lhs, rhs); // Calls processAccess with the extracted types
                },
                x,
                y);
        }

        // Add a task to the list, incremenets wait counter of task if task is not immidiately ready to run
        // TODO think about rvalue reference
        bool add_task(task_access&& task)
        {
            // add task to resrource queue
            // TODO think about empalce
            // TODO think about locking and when to insert (maybe insert after checking for blocking).
            // 2 threads cant add together
            // lastTask = tasks.insert_after(lastTask, task);
            // // TODO add to wait counter here
            bool ready = 0;

            // check ready
            if(firstNotReady == tasks.end())
            {
                // all previous tasks are ready
                // check if some ready task blocks me
                auto it = check_blocking(tasks.cbegin(), firstNotReady, task);
                if(it != firstNotReady)
                {
                    // someone is blocking, add and wait
                    lastTask = tasks.insert_after(lastTask, task);
                    firstNotReady = lastTask;
                    ++*task.waitCounter_p;
                    ready = 0;
                }
                else
                {
                    //  no one is blocking. Add as ready
                    lastTask = tasks.insert_after(lastTask, task);
                    // set to end all the time since end might change after adding a task
                    firstNotReady = tasks.end();
                    ready = 1;
                }
            }
            else
            {
                lastTask = tasks.insert_after(lastTask, task);
                // firstNotReady stays the same
                ++*task.waitCounter_p;
                ready = 0;
                // there is a not ready task before us, so we cannot be ready. Add and wait
            }
            std::cout << "Is task ready to execute " << ready << std::endl;

            return ready;
        }

        // Removes all active tasks from the list which have a handle
        // Task is not guaranteed to be present in this ResourceNode, may not be registered here
        // returns bool : true if there are no more tasks in the list, signal to delete the node
        template<typename TCoroHandle>
        bool remove_task(TCoroHandle handle, ThreadPool* pool_p)
        {
            // TODO use erase if or find if?
            // Since task was run, is done, and is being removed, it must have been in the running tasks
            // Assuming only one of a handle is added
            auto first = tasks.before_begin();
            auto next = std::next(first);

            while(next != firstNotReady)
            {
                // found handle here
                if(next->handle == handle)
                {
                    auto oldAccessMode = next->accessMode;
                    tasks.erase_after(first);
                    // erased after first.
                    // We have a new next. No need to increment first
                    update_ready(oldAccessMode, pool_p);
                }
                else
                {
                    ++first;
                }
                next = std::next(first);
            }
            return tasks.empty();
        }

        // Check if the ResourceNode is empty
        bool empty() const
        {
            return tasks.empty(); // Returns true if the list is empty
        }

        // TODO think about using const iterators
        // checks if someone between first and last blocks task
        // TODO this is a find_if
        // returns iterator to person who blocks or to the last if no one blocks
        // assumes there is atleast one element backwards because of where it is called from
        typename std::forward_list<task_access>::const_iterator check_blocking(
            typename std::forward_list<task_access>::const_iterator first,
            typename std::forward_list<task_access>::const_iterator last,
            task_access const& task)
        {
            while(first != last)
            {
                if(isSerial(task.accessMode, first->accessMode))
                {
                    // Stop when a serial access is found
                    break;
                }
                first++;
            }
            return first;
        }

        // task_access has been modified or deleted
        // starting from the firstNotReady task, check if task can be set to ready
        // TODO think of mutexes. Update, add,remove and checkBlocking
        // returns handle if it can be resumed,
        void update_ready(typename task_access::AccessModes old_access_mode, ThreadPool* pool_p)
        {
            auto it = firstNotReady;
            // try to update all not ready tasks in order
            while(it != tasks.end())
            {
                // modified_task was blocking it
                if(isSerial(old_access_mode, it->accessMode))
                {
                    // TODO think about splitting it up into begin -> firstNotReady and from firstNotReady to it
                    if(check_blocking(tasks.cbegin(), it, *it) != it)
                    {
                        // someone else is also blocking it
                        // dont set to ready
                        std::cout << "Someone else blocks me" << std::endl;
                        break;
                    }
                    else
                    {
                        // no one else is blocking
                        // set to ready
                        std::cout << "Check wait value : " << *(it->waitCounter_p) << std::endl;

                        if(--*(it->waitCounter_p) == 0)
                        {
                            std::cout << "Notify to make ready. Val : " << *(it->waitCounter_p) << std::endl;
                            // move handle to ready tasks queue
                            pool_p->addReadyTask(it->handle);
                        }
                        it++;
                    }
                }
                // modified task wasnt blocking it
                else
                {
                    // nothing to update, it wasn't blocked by modified task
                    // the ones who block must unblock
                    break;
                }
            }
            // it points one past the last ready task
            firstNotReady = it;
        }
    };
} // namespace rg