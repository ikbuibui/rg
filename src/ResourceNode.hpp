
#pragma once

#include "ThreadPool.hpp"
#include "resources.hpp"

#include <algorithm>
#include <atomic>
#include <coroutine>
#include <cstdint>
#include <functional>
#include <list>
#include <typeinfo>
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
        std::list<task_access> tasks{}; // List of task_access instances
        std::list<task_access>::iterator firstNotReady; // Iterator to the first not-ready task

        // Constructor
        ResourceNode(uint32_t uid) : resource_uid(uid), firstNotReady(tasks.end())
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

        // Add a task to the list
        bool add_task(task_access const& task)
        {
            // add task to resrource queue
            std::cout << "Size before add " << tasks.size() << std::endl;
            tasks.push_back(task);
            std::cout << "Size after add " << tasks.size() << std::endl;
            bool ready = 0;

            // check ready
            if(firstNotReady == tasks.end())
            {
                // all previous tasks are ready
                // check from the end, becuase we are the last element
                auto it = check_blocking_backwards(tasks.rbegin());
                if(it != tasks.rend())
                {
                    // someone is blocking
                    firstNotReady = std::prev(tasks.end());
                    ready = 0;
                }
                else
                {
                    ready = 1;
                }
            }
            else
            {
                ready = 0;
                // there is a not ready task before us, so we cannot be ready. Sit and chill
            }
            std::cout << "Is task ready to execute " << ready << std::endl;

            return ready;
        }

        // Remove a task from the list using erase_if
        // Task is not guaranteed to be present in this ResourceNode, may not be registered here
        // returns bool : true if there are no more tasks in the list, signal to delete the node
        template<typename TCoroHandle>
        bool remove_task(TCoroHandle handle, ThreadPool* pool_p)
        {
            // Since task was run, is done, and is being removed, it must have been in the running tasks
            auto it = std::find_if(
                tasks.begin(),
                firstNotReady,
                [&](task_access const& task) { return task.handle == handle; });

            // found handle here
            if(it != firstNotReady)
            {
                auto oldAccessMode = it->accessMode;
                std::cout << "Size before erase " << tasks.size() << std::endl;
                tasks.erase(it);
                std::cout << "Size after erase " << tasks.size() << std::endl;
                update_ready(oldAccessMode, pool_p);
            }
            return tasks.empty();
        }

        // Check if the ResourceNode is empty
        bool empty() const
        {
            return tasks.empty(); // Returns true if the list is empty
        }

        // TODO think about using const iterators
        // returns iterator to person who blocks or rend if no one blocks
        // assumes there is atleast one element backwards because of where it is called from
        typename std::list<task_access>::reverse_iterator check_blocking_backwards(
            typename std::list<task_access>::reverse_iterator start)
        {
            // Convert the iterator to a reverse_iterator
            auto it = start;
            // safe due to assumption
            if(it == tasks.rend())
            {
                return it;
            }
            ++it;
            while(it != tasks.rend())
            {
                if(isSerial(start->accessMode, it->accessMode))
                {
                    // Stop when a serial access is found
                    break;
                }
                ++it;
            }
            return it;
        }

        // task_access has to be modified or deleted
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
                    // check previous
                    if(check_blocking_backwards(std::make_reverse_iterator(it)) != tasks.rend())
                    {
                        // someone else is also blocking it
                        // dont set to ready
                        std::cout << "Someone else blocks me" << std::endl;
                        firstNotReady = it;
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
