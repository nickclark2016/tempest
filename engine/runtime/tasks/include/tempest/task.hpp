#ifndef tempest_tasks_task_hpp
#define tempest_tasks_task_hpp

#include <tempest/api.hpp>
#include <tempest/assert.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/task_arena.hpp>

namespace tempest
{
    enum class task_priority : uint8_t
    {
        low = 0,
        normal = 1,
        high = 2,
        critical = 3,
    };

    enum class pressure_state : uint8_t
    {
        nominal = 0,
        moderate = 1,
        high = 2,
        critical = 3,
    };

    using task_urgency = uint8_t;

    struct TEMPEST_API task_metadata
    {
        static constexpr task_urgency default_urgency = 128; // Midpoint urgency value.

        task_priority priority = task_priority::normal;
        task_urgency urgency = default_urgency;
        bool is_degraded = false;
    };

    class TEMPEST_API scoped_task_group;

    struct TEMPEST_API coro_promise_base
    {
        struct allocation_header
        {
            static constexpr uint64_t memory_guard_value = 0xDEADBEEFDEADC0DE; // NOLINT

            task_memory_arena* arena;
            uint64_t memory_guard = memory_guard_value;
        };

        static auto operator new(size_t size, task_memory_arena& arena) noexcept -> void*;
        static auto operator delete(void* ptr, size_t size) noexcept -> void;

        [[nodiscard]] auto initial_suspend() const noexcept -> suspend_always // NOLINT
        {
            return {};
        }

        struct final_awaiter
        {
            [[nodiscard]] auto await_ready() const noexcept -> bool // NOLINT
            {
                return false;
            }

            [[nodiscard]] auto await_suspend(coroutine_handle<coro_promise_base> handle) const noexcept
                -> coroutine_handle<>;

            auto await_resume() const noexcept -> void
            {
            }
        };

        auto unhandled_exception() -> void;

        task_metadata metadata;
        scoped_task_group* group = nullptr;
        coroutine_handle<> continuation;
    };

    template <typename T = void>
    class TEMPEST_API task;

    template <typename T>
    class TEMPEST_API task
    {
      public:
        class promise_type : public coro_promise_base
        {
          public:
            auto get_return_object() -> task<T>
            {
                return task{coroutine_handle<promise_type>::from_address(this)};
            }

            auto return_value(T val) -> void
            {
                _value.emplace(tempest::move(val));
            }

            [[nodiscard]] auto final_suspend() const noexcept -> final_awaiter
            {
                return {};
            }

            auto value() & -> T&
            {
                return *_value;
            }

            auto value() const& -> const T&
            {
                return *_value;
            }

            auto value() && -> T&&
            {
                return tempest::move(*_value);
            }

            auto value() const&& -> const T&&
            {
                return tempest::move(*_value);
            }

            [[nodiscard]] auto has_value() const noexcept -> bool
            {
                return _value.has_value();
            }

          private:
            optional<T> _value;
        };

        task() noexcept = default;

        task(coroutine_handle<promise_type> handle) noexcept : _handle(handle)
        {
        }

        task(const task&) = delete;

        task(task&& other) noexcept : _handle(exchange(other._handle, noop_coroutine()))
        {
        }

        ~task()
        {
            if (_handle)
            {
                _handle.destroy();
            }
        }

        task& operator=(const task&) = delete; // NOLINT

        task& operator=(task&& other) noexcept // NOLINT
        {
            if (this != &other)
            {
                if (_handle)
                {
                    _handle.destroy();
                }
                _handle = exchange(other._handle, noop_coroutine());
            }
            return *this;
        }

        [[nodiscard]] auto await_ready() const noexcept -> bool
        {
            return _handle.done();
        }

        auto await_suspend(coroutine_handle<> awaiting_coro) noexcept -> void
        {
            _handle.promise().continuation = awaiting_coro;
            // TODO: Enqueue this task for execution in the task scheduler.
        }

        [[nodiscard]] auto await_resume() -> T
        {
            TEMPEST_ASSERT(_handle.promise().has_value() &&
                           "Attempting to retrieve value from a task that has no value set.");
            return _handle.promise().value();
        }

      private:
        coroutine_handle<promise_type> _handle;
    };

    template <>
    class TEMPEST_API task<void>
    {
      public:
        struct promise_type : public coro_promise_base
        {
            auto get_return_object() -> task<void>
            {
                return task<void>{coroutine_handle<promise_type>::from_address(this)};
            }

            auto return_void() -> void
            {
            }

            [[nodiscard]] auto final_suspend() const noexcept -> final_awaiter // NOLINT
            {
                return {};
            }
        };

        task(coroutine_handle<promise_type> handle) noexcept : _handle(handle)
        {
        }

        task(const task&) = delete;

        task(task&& other) noexcept : _handle(exchange(other._handle, nullptr))
        {
        }

        ~task()
        {
            if (_handle)
            {
                _handle.destroy();
            }
        }

        task& operator=(const task&) = delete; // NOLINT

        task& operator=(task&& other) noexcept // NOLINT
        {
            if (this != &other)
            {
                if (_handle)
                {
                    _handle.destroy();
                }
                _handle = exchange(other._handle, nullptr);
            }
            return *this;
        }

        [[nodiscard]] auto await_ready() const noexcept -> bool
        {
            return _handle.done();
        }

        auto await_suspend(coroutine_handle<> awaiting_coro) noexcept -> void
        {
            _handle.promise().continuation = awaiting_coro;
            // TODO: Enqueue this task for execution in the task scheduler.
        }

        void await_resume() const noexcept
        {
        }

      private:
        coroutine_handle<promise_type> _handle;
    };

    class TEMPEST_API scoped_task_group
    {
      public:
        void on_task_complete();
        void cancel();

      private:
    };
} // namespace tempest

#endif // tempest_tasks_task_hpp
