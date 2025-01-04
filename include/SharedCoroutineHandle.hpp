#pragma once

#include <atomic>
#include <cassert>
#include <coroutine>
#include <cstddef>
#include <stdexcept>

namespace rg
{

    // Note this class should not be accessed after being moved from or reset
    class SharedCoroutineHandle
    {
    public:
        using TRefCount = uint32_t;

        template<typename PromiseType>
        explicit SharedCoroutineHandle(
            std::coroutine_handle<PromiseType> handle,
            std::atomic<TRefCount>& ref_count) noexcept
            : address_(handle.address())
            , ref_count_(&ref_count)
        {
        }

        explicit SharedCoroutineHandle(void* address, std::atomic<TRefCount>& ref_count) noexcept
            : address_(address)
            , ref_count_(&ref_count)
        {
        }

        explicit SharedCoroutineHandle() noexcept : address_(nullptr), ref_count_(nullptr)
        {
        }

        ~SharedCoroutineHandle()
        {
            decrement_ref();
        }

        SharedCoroutineHandle(SharedCoroutineHandle const& other) noexcept
            : address_(other.address_)
            , ref_count_(other.ref_count_)
        {
            increment_ref();
        }

        SharedCoroutineHandle(SharedCoroutineHandle&& other) noexcept
            : address_(std::move(other.address_))
            , ref_count_(std::move(other.ref_count_))
        {
            other.address_ = nullptr;
            other.ref_count_ = nullptr;
        }

        // If this is the last object alive, and you assign to itself, it will burn
        SharedCoroutineHandle& operator=(SharedCoroutineHandle const& other) noexcept
        {
            if(this != &other)
            {
                decrement_ref();
                address_ = other.address_;
                ref_count_ = other.ref_count_;
                increment_ref();
            }
            return *this;
        }

        // If this is the last object alive, and you assign to itself, it will burn
        SharedCoroutineHandle& operator=(SharedCoroutineHandle&& other) noexcept
        {
            if(this != &other)
            {
                decrement_ref();
                address_ = other.address_;
                ref_count_ = other.ref_count_;
                other.address_ = nullptr;
                other.ref_count_ = nullptr;
            }

            return *this;
        }

        // doesn't use decrement and setting address and refcnt to nullptr, to avoid use after free, in case the handle
        // is destroyed on decrement (since handle might be held in the coroutine frame)
        void reset()
        {
            auto local_address_copy = address_;
            auto local_ref_count_copy = ref_count_;

            address_ = nullptr;
            ref_count_ = nullptr;

            if(local_ref_count_copy && local_ref_count_copy->fetch_sub(1) == 1)
            {
                std::coroutine_handle<>::from_address(local_address_copy).destroy();
            }
        }

        // Checks if the handle is valid
        explicit operator bool() const noexcept
        {
            return address_ != nullptr;
        }

        // checks if the object was ever initialized. It may be in an invalid state
        bool is_init() const noexcept
        {
            return address_ != nullptr;
        }

        template<typename PromiseType>
        std::coroutine_handle<PromiseType> get_coroutine_handle() const
        {
            return std::coroutine_handle<PromiseType>::from_address(address_);
        }

        std::coroutine_handle<> get_coroutine_handle() const
        {
            return std::coroutine_handle<>::from_address(address_);
        }

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

        std::atomic<TRefCount>* use_count_ptr() const noexcept
        {
            return ref_count_;
        }


    private:
        void* address_; // Raw pointer to the coroutine handle
        std::atomic<TRefCount>* ref_count_; // Pointer to the thread-safe reference count

        void destroy_coroutine()
        {
            std::coroutine_handle<>::from_address(address_).destroy();
        }

        // TODO remove the if(ref_count) and setting to nullptr from increment and decrement

        // Increment the reference count
        void increment_ref()
        {
            if(ref_count_)
                ref_count_->fetch_add(1, std::memory_order_relaxed);
        }

        // Decrement the reference count and clean up if it reaches zero
        void decrement_ref()
        {
            if(ref_count_ && ref_count_->fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                ref_count_ = nullptr;

                destroy_coroutine();
            }
        }
    };
} // namespace rg
