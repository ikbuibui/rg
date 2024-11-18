#pragma once

#include "ResourceNode.hpp"
#include "ThreadPool.hpp"
#include "dispatchTask.hpp"
#include "resources.hpp"

#include <coroutine>
#include <cstdint>
#include <list>
#include <mutex>

namespace rg
{

    // I only need the taskspace for siblings.
    // Object to be held inside the promise object of the task
    // is the lifetime okay? no. need to keep the coroutine alive if I keep it in the promise.
    // Try keeping the taskSpace inside the Task for now. and keep task alive
    //  suspend_always, unless task space is empty and it returns void
    // else use a shared ptr to a task space in the task and pass this shared pointer around
    struct ExecutionSpace
    {
        std::list<ResourceNode> resList{}; // list to hold resources accesses of children
        ThreadPool* pool_p{};
        // std::list<std::list<RA::mode>> resList{};

        // list of resources it is registed in the parent (dont need access mode, only UID i think)

        bool deregisterOnDone = false;
        bool destroyOnDone = false;

        // coroutine handle or parent space ptrcan be used to signal deregistration required
        std::coroutine_handle<> ownerHandle = nullptr;
        ExecutionSpace* parentSpace = nullptr;

        std::mutex mtx;

        // checks if resList holding children is empty
        bool done()
        {
            std::lock_guard lock(mtx);

            // this doesnt guarantee that the task is done, we may create new children inside a currently paused
            // continuation. To check if task is done, check if it is in final suspend and then check if children are
            // done. Still we cannot delete resources if get hasnt been called
            //
            return resList.empty();
        }

        // only called when resList is empty
        ~ExecutionSpace()
        {
            // deregister from resources in parent space

            // destroy task which holds this space

            // destroy the space
        }

        // called by a child task
        // removes the child task from the resList
        // if resList is now empty and task is done and get is done, destroy
        template<typename TCoroHandle>
        void deregister(TCoroHandle handle)
        {
            std::lock_guard lock(mtx);

            auto it = resList.begin();
            while(it != resList.end())
            {
                auto temp = it;
                bool nodeEmpty = it->remove_task(handle, pool_p);
                ++it;
                if(nodeEmpty)
                {
                    resList.erase(temp);
                }
            }
            // last task which is removed from the space will do this
            // space is done. do conditional deregister and destroy
            if(deregisterOnDone && done())
            {
                parentSpace->deregister(ownerHandle);
            }
            if(destroyOnDone && done())
            {
                ownerHandle.destroy();
            }
        }

        // Can be done with forwarding Args as well.
        // template<typename... ResourceAccess>
        // auto addDependencies(std::coroutine_handle<> h, ResourceAccess const&... ras)
        // {
        //     uint32_t waitCount = 0;
        //     // for ra:ras
        //     //     ready += addDependency(h, ra)
        //     return waitCount;
        // }

        // return number of notifications it needs to wait fort
        template<typename TCoroHandle, typename TDeferredCallable>
        auto addDependencies(TCoroHandle h, TDeferredCallable const&)
        {
            std::lock_guard lock(mtx);

            uint32_t waitCount = 0;

            // go over type list of resourceAccess in dc and for each resource accessm add dependency
            TDeferredCallable::ResourceAccessList::for_each([this, &waitCount, h]<typename RA>()
                                                            { waitCount += addDependency(h, RA{}); });

            std::cout << "wait Count " << waitCount << std::endl;
            return waitCount;
        }


    private:
        // Resource Access has a uid and
        template<typename TCoroHandle, typename ResourceAccess>
        auto addDependency(TCoroHandle h, ResourceAccess const&)
        {
            std::cout << "called add dependency" << std::endl;
            // Locate or create the ResourceNode
            auto resIt = std::find_if(
                resList.begin(),
                resList.end(),
                [](ResourceNode& node)
                {
                    std::cout << "resource UID " << node.resource_uid << std::endl;

                    return node.resource_uid == ResourceAccess::resource_id;
                });

            // If not found, create a new ResourceNode with the given UID and add to the list
            if(resIt == resList.end())
            {
                resIt = resList.emplace(resList.end(), ResourceAccess::resource_id);
            }

            // Create a task_access for the coroutine handle and access mode
            // Add the task to the ResourceNode's queue and check readiness
            bool isReady = resIt->add_task({h, typename ResourceAccess::access_type{}, &h.promise().waitCounter});

            // Return 1 if the task must wait for execution
            return !isReady;
            // iterate and find if resource of ra exists else add res to list
            // if resource exists, goto its task queue
            // if resource doesnt exist create task queue
            // auto& queue = getResourceQueue(ra, resourceList);

            // adds (access type, coro handle) to the resource queue
            // returns if it is at the head of the queue (ready to execute)
            // bool ready = addToQueue(h, ResourceAccess::access_type);

            // return if it is ready or needs to wait to be notified by this resource
        }
    };
} // namespace rg
