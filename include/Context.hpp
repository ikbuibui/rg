#pragma once

#include "SharedCoroutineHandle.hpp"
#include "ThreadPool.hpp"

#include <functional>

namespace rg
{
    struct Context
    {
        std::reference_wrapper<SharedCoroutineHandle> handleRef;
        ThreadPool* poolPtr;

        Context(SharedCoroutineHandle& handle, ThreadPool* ptr) : handleRef(handle), poolPtr(ptr)
        {
        }
    };
} // namespace rg
