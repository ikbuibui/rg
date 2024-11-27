#pragma once

#include <coroutine>
#include <memory>

// Custom deleter for coroutine handles
template<typename PromiseType>
void coroutine_handle_deleter(void* address)
{
    auto handle = std::coroutine_handle<PromiseType>::from_address(address);
    // deregister from resources
    // decrement child task counter of the parent
    if(--handle.promise().parent.promise().childCounter == 0)
    {
        // if counter hits zero, notify cv
        handle.promise().cv.notify_all();
    }
    // destroy
    handle.destroy();
}

// Shared handle wrapper around coroutine_handle
template<typename PromiseType>
class SharedCoroutineHandle
{
public:
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

    void resume() const
    {
        auto handle = get_coroutine_handle();
        if(handle)
        {
            handle.resume();
        }
    }

    bool done() const noexcept
    {
        auto handle = get_coroutine_handle();
        return handle ? handle.done() : true;
    }

    PromiseType& promise() const
    {
        auto handle = get_coroutine_handle();
        if(!handle)
        {
            throw std::runtime_error("Invalid coroutine handle");
        }
        return handle.promise();
    }

    std::coroutine_handle<PromiseType> getHandle() const
    {
        return std::coroutine_handle<PromiseType>::from_address(handle_.get());
    }


private:
    // Retrieve the internal coroutine_handle
    std::coroutine_handle<PromiseType> get_coroutine_handle() const
    {
        return std::coroutine_handle<PromiseType>::from_address(handle_.get());
    }

    std::shared_ptr<void> handle_;
};
