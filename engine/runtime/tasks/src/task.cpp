#include <tempest/coroutine.hpp>
#include <tempest/task.hpp>

#include <tempest/assert.hpp>

namespace tempest
{
    auto coro_promise_base::operator new(size_t size, task_memory_arena& arena) noexcept -> void*
    {
        auto* const header = static_cast<allocation_header*>(arena.allocate(sizeof(allocation_header) + size));
        if (header == nullptr)
        {
            return nullptr;
        }

        header->arena = &arena;
        header->memory_guard = allocation_header::memory_guard_value;

        return reinterpret_cast<void*>(header + 1);
    }

    auto coro_promise_base::operator delete(void* ptr, size_t size) noexcept -> void
    {
        if (ptr == nullptr)
        {
            return;
        }

        auto* const header = reinterpret_cast<allocation_header*>(ptr) - 1;
        if (header->memory_guard != allocation_header::memory_guard_value)
        {
            // Memory corruption detected. Handle this as appropriate (e.g., log an error, trigger a breakpoint, etc.).
            TEMPEST_ASSERT(false && "Memory corruption detected in coroutine promise allocation!");
            return;
        }

        header->arena->deallocate(header, sizeof(allocation_header) + size);
    }

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static) - Must be a non-static member function to be used as a final awaiter.
    auto coro_promise_base::final_awaiter::await_suspend(coroutine_handle<coro_promise_base> handle) const noexcept -> coroutine_handle<>
    {
        auto& promise = handle.promise();
        if (promise.group != nullptr) {
            promise.group->on_task_complete();
        }

        return promise.continuation ? promise.continuation : noop_coroutine();
    }

    auto coro_promise_base::unhandled_exception() -> void
    {
        if (group != nullptr)
        {
            group->cancel();
        }
    }

    auto scoped_task_group::on_task_complete() -> void
    {
    }

    auto scoped_task_group::cancel() -> void
    {
    }
} // namespace tempest