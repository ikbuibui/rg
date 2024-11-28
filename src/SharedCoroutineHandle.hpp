#pragma once

#include <algorithm>
#include <coroutine>
#include <memory>

// Custom deleter for coroutine handles
template<typename PromiseType>
void coroutine_handle_deleter(void* address)
{
    auto handle = std::coroutine_handle<PromiseType>::from_address(address);
    // deregister from resources
    auto& promise = handle.promise();
    std::ranges::for_each(
        promise.resourceNodes,
        [handle, &promise](auto const& resNode) { resNode->remove_task(handle, promise.pool_p); });

    // decrement child task counter of the parent
    if(--*handle.promise().parentChildCounter == 0)
    {
        // if counter hits zero, notify parent cv
        // handle.promise().cv.notify_all();
    }
    // destroy
    handle.destroy();
}

// Shared handle wrapper around coroutine_handle
class SharedCoroutineHandle
{
public:
    template<typename PromiseType>
    explicit SharedCoroutineHandle(std::coroutine_handle<PromiseType> handle)
        : handle_(std::shared_ptr<void>(handle.address(), coroutine_handle_deleter<PromiseType>))
    {
    }

    explicit SharedCoroutineHandle() : handle_(nullptr)
    {
    }

    // Checks if the handle is valid
    explicit operator bool() const noexcept
    {
        return handle_ != nullptr;
    }

    void reset()
    {
        handle_.reset();
    }

    // void resume() const
    // {
    //     auto handle = get_coroutine_handle();
    //     if(handle)
    //     {
    //         handle.resume();
    //     }
    // }

    template<typename PromiseType>
    PromiseType& promise() const
    {
        auto handle = get_coroutine_handle<PromiseType>();
        if(!handle)
        {
            throw std::runtime_error("Invalid coroutine handle");
        }
        return handle.promise();
    }

    template<typename PromiseType>
    std::coroutine_handle<PromiseType> get_coroutine_handle() const
    {
        return std::coroutine_handle<PromiseType>::from_address(handle_.get());
    }

    std::coroutine_handle<> get_coroutine_handle() const
    {
        return std::coroutine_handle<>::from_address(handle_.get());
    }

private:
    // Retrieve the internal coroutine_handle

    std::shared_ptr<void> handle_;
};
