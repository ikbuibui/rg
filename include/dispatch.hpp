#include "Context.hpp"
#include "DispatchAwaiter.hpp"
#include "Registration.hpp"
#include "ResourceAccess.hpp"
#include "ThreadPool.hpp"

#include <atomic>
#include <functional>

namespace rg
{
    // Helper lambda to transform each accessHandle
    auto transform_resource(auto&& arg) -> decltype(auto)
    {
        if constexpr(IsTransformResourceAccess<std::decay_t<decltype(arg)>>)
        {
            return arg();
        }
        else
        {
            return std::forward<decltype(arg)>(arg);
        }
    };

    template<typename Callable, typename... Args>
    auto invoke_register(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args)
    {
        ctx.poolPtr = pool_p;
        auto handle = std::invoke(std::forward<Callable>(task), std::move(ctx), transform_resource(args)...);

        // register to the resources
        // if it is a transform resource, call transform on it
        constexpr uint16_t resource_counter = (static_cast<uint16_t>(IsResourceAccess<Args>) + ... + 0);
        auto& handlePromise = handle.coro.template promise<typename decltype(handle)::promise_type>();
        auto& waitCounter = handlePromise.waitCounter;
        waitCounter.fetch_add(resource_counter, std::memory_order_relaxed);

        auto rawCoroHandle = handle.coro.template get_coroutine_handle<typename decltype(handle)::promise_type>();

        handlePromise.registration
            = Registration{resource_counter, rawCoroHandle, waitCounter, ctx.poolPtr, std::forward<Args>(args)...};

        return std::move(handle);
    }

    // TODO add requires ReturnsTask<Callable, Args...>
    template<bool synchronous = false, bool finishedOnReturn = false, typename Callable, typename... Args>
    auto dispatch_task(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args)
    {
        auto handle = invoke_register(pool_p, ctx, std::forward<Callable>(task), std::forward<Args>(args)...);
        return DispatchAwaiter<decltype(handle), synchronous, finishedOnReturn>{std::move(handle), ctx.poolPtr};
    }

    template<bool synchronous = false, bool finishedOnReturn = false, typename Callable, typename... Args>
    auto dispatch_task(Context ctx, Callable&& task, Args&&... args)
    {
        return dispatch_task<synchronous, finishedOnReturn>(
            ctx.poolPtr,
            ctx,
            std::forward<Callable>(task),
            std::forward<Args>(args)...);
    }
} // namespace rg
