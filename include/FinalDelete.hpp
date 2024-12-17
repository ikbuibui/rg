#pragma once

#include "SharedCoroutineHandle.hpp"

namespace rg
{
    struct FinalDelete
    {
        SharedCoroutineHandle self;

        constexpr bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<>) noexcept
        {
            // std::cout << "reset handle with use count : " << self.use_count() << std::endl;
            // Can i do transfer here? if the task is going to be deleted, and the when removed from resources and it
            // makes another task ready for resources, can i start executing this task? but it may make multiple tasks
            // ready. Maybe return one task
            self.reset();
        }

        constexpr void await_resume() const noexcept
        {
        }
    };
} // namespace rg
