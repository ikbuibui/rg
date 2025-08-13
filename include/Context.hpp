#pragma once

#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"

#include <functional>

namespace rg
{
    // By default the conext is passed on inside the task and task set itself as the parent by setting its own handle
    // in the handleRef. The child task is by default then set to be allocated on the same pool as the parent task. If
    // the user wants to change this,
    struct Context
    {
        std::reference_wrapper<SharedCoroutineHandle> handleRef;
        ThreadPool* poolPtr;

        Context(SharedCoroutineHandle& handle, ThreadPool* ptr) : handleRef(handle), poolPtr(ptr)
        {
        }
    };
} // namespace rg
