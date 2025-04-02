    template<typename T>
    struct SyncTask
    {
        struct promise_type
        {
            T value;
            std::exception_ptr exception;

            SyncTask get_return_object()
            {
                return SyncTask{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_never initial_suspend() noexcept
            {
                return {};
            }

            std::suspend_never final_suspend() noexcept
            {
                return {};
            }

            void return_value(T v)
            {
                value = v;
            }

            void unhandled_exception()
            {
                exception = std::current_exception();
            }
        };

        std::coroutine_handle<promise_type> coro;

        SyncTask(std::coroutine_handle<promise_type> h) : coro(h)
        {
        }

        ~SyncTask()
        {
            if(coro)
                coro.destroy();
        }

        T get()
        {
            if(coro.promise().exception)
                std::rethrow_exception(coro.promise().exception);
            return coro.promise().value;
        }
    };

    template<>
    struct SyncTask<void>
    {
        struct promise_type
        {
            std::exception_ptr exception;

            SyncTask get_return_object()
            {
                return SyncTask{std::coroutine_handle<promise_type>::from_promise(*this)};
            }

            std::suspend_never initial_suspend() noexcept
            {
                return {};
            }

            std::suspend_never final_suspend() noexcept
            {
                return {};
            }

            void return_void()
            {
            }

            void unhandled_exception()
            {
                exception = std::current_exception();
            }
        };

        std::coroutine_handle<promise_type> coro;

        SyncTask(std::coroutine_handle<promise_type> h) : coro(h)
        {
        }

        ~SyncTask()
        {
            if(coro)
                coro.destroy();
        }

        void get()
        {
            if(coro.promise().exception)
                std::rethrow_exception(coro.promise().exception);
        }
    };

    template<bool Synchronous = false, bool finishedOnReturn = false, typename Callable, typename... ResourceAccess>
    auto dispatch_task_sync(Callable&& callable, ResourceAccess&&... accessHandles)
        -> SyncTask<decltype(callable(std::forward<ResourceAccess>(accessHandles)...).get())>
    {
        auto result = co_await dispatch_task<Synchronous, finishedOnReturn>(
            std::forward<Callable>(callable),
            std::forward<ResourceAccess>(accessHandles)...);
        co_return result;
    }
