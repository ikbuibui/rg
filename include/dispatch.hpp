#include "Context.hpp"
#include "DispatchAwaiter.hpp"
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
        auto& resourceUsage = handlePromise.resourceUsage;
        auto& waitCounter = handlePromise.waitCounter;

        resourceUsage.reserve(resource_counter);
        waitCounter.fetch_add(resource_counter, std::memory_order_relaxed);
        // Register task to resources
        // Fold expression only for handles satisfying HasAccessType
        (...,
         (
             [&](auto& arg)
             {
                 if constexpr(IsResourceAccess<decltype(arg)>)
                 {
                     resourceUsage.emplace_back(
                         &arg.resource.getResNode().userQueue,
                         arg.resource.getResNode().userQueue.add_task(
                             {handle.coro.template get_coroutine_handle<typename decltype(handle)::promise_type>(),
                              arg.getAccessMode(),
                              &waitCounter,
                              ctx.poolPtr}));
                 }
             }(args)));

        return std::move(handle);
    }

    // TODO add requires ReturnsTask<Callable, Args...>
    template<bool synchronous = false, bool finishedOnReturn = false, typename Callable, typename... Args>
    auto dispatch_task(ThreadPool* pool_p, Context ctx, Callable&& task, Args&&... args)
    {
        auto handle = invoke_register(pool_p, ctx, std::forward<Callable>(task), std::forward<Args>(args)...);
        return DispatchAwaiter<decltype(handle), synchronous, finishedOnReturn>{std::move(handle)};
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
