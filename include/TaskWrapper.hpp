#include <coroutine>

namespace rg
{
    template<typename TaskHandle>
    struct TaskWrapper
    {
        struct promise_type
        {
            promise_type(auto&... args)
            {
            }

            TaskWrapper get_return_object() noexcept
            {
                return {};
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
            }
        };

        TaskHandle handle;
    };
} // namespace rg
