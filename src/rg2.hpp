#include <coroutine>
#include <iostream>
#include <optional>
#include <utility>

template<typename T>
struct Task {
    struct promise_type {
        std::optional<T> result;

        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        void unhandled_exception() { std::terminate(); } // Customize error handling as needed

        template<typename U>
        void return_value(U&& value) {
            result = std::forward<U>(value);
        }
    };

    std::coroutine_handle<promise_type> coro;

    explicit Task(std::coroutine_handle<promise_type> h) : coro(h) {}
    ~Task() { if (coro) coro.destroy(); }

    T get() {
        return coro.promise().result.value();
    }
};

// Coroutine function template that wraps the callable
template<typename Callable, typename... Args>
auto wrap_in_coroutine(Callable&& callable, Args&&... args) -> Task<decltype(callable(std::forward<Args>(args)...))> {
    co_return callable(std::forward<Args>(args)...);
}

// Function that accepts a callable, calls the coroutine, and returns the result
template<typename Callable, typename... Args>
auto call_and_return_coroutine_result(Callable&& callable, Args&&... args) {
    auto task = wrap_in_coroutine(std::forward<Callable>(callable), std::forward<Args>(args)...);
    return task.get(); // Calls the coroutine and immediately gets the result
}

// Example callable
int compute(int a, int b) {
    return a + b;
}
