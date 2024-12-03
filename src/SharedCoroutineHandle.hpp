#pragma once

#include <coroutine>
#include <iostream>
#include <memory>

// Custom deleter for coroutine handles
// dont need the promise type do cleanup in the promise destrouctor
template<typename PromiseType>
void coroutine_handle_deleter(void* address)
{
    auto handle = std::coroutine_handle<PromiseType>::from_address(address);
    // destroy
    std::cout << "destroy handle" << std::endl;
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

    // SharedCoroutineHandle(SharedCoroutineHandle const&) = default;
    // SharedCoroutineHandle(SharedCoroutineHandle&&) = default;
    // SharedCoroutineHandle& operator=(SharedCoroutineHandle const&) = default;
    // SharedCoroutineHandle& operator=(SharedCoroutineHandle&&) = default;

    // ~SharedCoroutineHandle()
    // {
    //     std::cout << "delete shared handle" << std::endl;
    // }

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

    auto use_count() const noexcept
    {
        return handle_.use_count();
    }

private:
    // Retrieve the internal coroutine_handle

    std::shared_ptr<void> handle_;
};
