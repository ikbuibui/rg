#pragma once

#include "SharedCoroutineHandle.hpp"

#include <coroutine>

namespace rg
{
    struct FinalDelete
    {
        SharedCoroutineHandle self;
        std::coroutine_handle<> continuationHandle = std::noop_coroutine();

        constexpr bool await_ready() const noexcept
        {
            return false;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept
        {
            // std::cout << "reset handle with use count : " << self.use_count() << std::endl;
            // Can i do transfer here? if the task is going to be deleted, and the when removed from resources and it
            // makes another task ready for resources, can i start executing this task? but it may make multiple tasks
            // ready. Maybe return one task
            auto localHandleCopy = continuationHandle;
            self.reset();
            return localHandleCopy;
        }

        constexpr void await_resume() const noexcept
        {
        }
    };
} // namespace rg
