#pragma once

#include "ResourceAccess.hpp"
#include "ResourceTaskQueue.hpp"
#include "ThreadPool.hpp"

#include <coroutine>
#include <vector>

namespace rg
{
    // Registers and deregisters resources for a task to rg
    // Move only type
    // If registrations need to be shared, use shared_ptr to Registration
    // Does not deregister resources on destruction. Deregister must be called on a Registration object
    struct Registration
    {
        // requires stable pointer access to taskData
        std::vector<std::pair<ResourceTaskQueue*, TaskData*>> resourceUsage;

        Registration() = default;

        Registration(
            uint16_t resCounter,
            std::coroutine_handle<> coroHandle,
            std::atomic<TWaitCount>& waitCounter,
            ThreadPool* poolPtr,
            auto&&... args)
        {
            resourceUsage.reserve(resCounter);

            // Register task to resources
            // Fold expression only for handles satisfying HasAccessType
            (...,
             (
                 [&](auto& arg)
                 {
                     if constexpr(IsResourceAccess<decltype(arg)>)
                     {
                         auto& userQueue = arg.resource.getResNode().userQueue;
                         resourceUsage.emplace_back(
                             &userQueue,
                             userQueue.add_task({coroHandle, arg.getAccessMode(), &waitCounter, poolPtr}));
                     }
                 }(args)));
        }

        ~Registration() = default;

        Registration(Registration const&) = delete;
        Registration(Registration&&) = default;
        Registration& operator=(Registration const&) = delete;
        Registration& operator=(Registration&&) = default;

        void deregister()
        {
            for(auto& [queue, task] : resourceUsage)
            {
                queue->remove_task(task);
            }
        }
    };
} // namespace rg
