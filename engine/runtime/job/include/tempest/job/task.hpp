#ifndef tempest_job_task_hpp
#define tempest_job_task_hpp

#include <tempest/api.hpp>
#include <tempest/assert.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/job/allocator.hpp>
#include <tempest/job/types.hpp>
#include <tempest/optional.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    template <typename T = void, typename E = job_error>
    class [[nodiscard]] task;

    namespace detail
    {
        struct promise_allocator_base
        {
            static auto operator new(size_t size) -> void*
            {
                return job_allocator::get_current()->allocate(size);
            }

            static auto operator delete(void* ptr, size_t size) noexcept -> void
            {
                job_allocator::deallocate(ptr, size);
            }
        };

        template <typename Promise>
        struct task_final_awaiter
        {
            [[nodiscard]] constexpr auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<Promise> h) noexcept -> coroutine_handle<>
            {
                auto cont = h.promise().continuation;
                return cont ? cont : noop_coroutine();
            }

            constexpr auto await_resume() const noexcept -> void
            {
            }
        };

        struct error_propagator
        {
            virtual ~error_propagator() = default;
            virtual auto propagate_error(job_error err) -> void = 0;
            virtual auto get_continuation() -> coroutine_handle<> = 0;
            virtual auto destroy_frame() -> void = 0;
        };
    } // namespace detail

    template <typename T, typename E>
    struct task_awaiter
    {
        task<T, E> awaited_task;

        [[nodiscard]] auto await_ready() const noexcept -> bool
        {
            return awaited_task.is_ready();
        }

        template <typename CallerPromise>
        auto await_suspend(coroutine_handle<CallerPromise> h) noexcept -> coroutine_handle<>
        {
            awaited_task.handle().promise().continuation = h;
            if constexpr (requires { h.promise().parent_propagator; })
            {
                awaited_task.handle().promise().parent_propagator = &h.promise();
            }
            return awaited_task.handle();
        }

        auto await_resume()
        {
            if constexpr (!is_void_v<T>)
            {
                return move(awaited_task.value());
            }
        }
    };

    // =========================================================================
    // Fallible task with value: task<T, E> (where T != void, E != void)
    // =========================================================================
    template <typename T, typename E>
    class [[nodiscard]] task
    {
      public:
        using value_type = T;
        using error_type = E;
        using result_type = expected<T, E>;

        struct promise_type : detail::promise_allocator_base, detail::error_propagator
        {
            task* owner{nullptr};
            result_type result{unexpected<E>{E::none}};
            coroutine_handle<> continuation{nullptr};
            detail::error_propagator* parent_propagator{nullptr};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> suspend_always
            {
                return {};
            }

            auto final_suspend() noexcept -> detail::task_final_awaiter<promise_type>
            {
                return {};
            }

            auto return_value(T value) noexcept -> void
            {
                result = move(value);
            }

            auto return_value(result_type res) noexcept -> void
            {
                result = move(res);
            }

            auto return_value(unexpected<E> err) noexcept -> void
            {
                result = move(err);
            }

            auto unhandled_exception() noexcept -> void
            {
                TEMPEST_ASSERT(false);
            }

            auto propagate_error(job_error err) -> void override
            {
                result = unexpected<E>(static_cast<E>(err));
                if (owner != nullptr)
                {
                    owner->_saved_result = result;
                    owner->_is_completed = true;
                    owner->_handle = nullptr;
                }
                if (parent_propagator != nullptr)
                {
                    parent_propagator->propagate_error(err);
                }
            }

            auto get_continuation() -> coroutine_handle<> override
            {
                if (parent_propagator != nullptr)
                {
                    return parent_propagator->get_continuation();
                }
                return continuation ? continuation : noop_coroutine();
            }

            auto destroy_frame() -> void override
            {
                auto* parent = parent_propagator;
                auto h = coroutine_handle<promise_type>::from_promise(*this);
                h.destroy();
                if (parent != nullptr)
                {
                    parent->destroy_frame();
                }
            }

            // Await transform for child fallible task
            template <typename ChildT, typename ChildE>
            auto await_transform(task<ChildT, ChildE>&& child)
            {
                struct child_task_awaiter
                {
                    task<ChildT, ChildE> child_task;

                    auto await_ready() const noexcept -> bool
                    {
                        return child_task.is_ready();
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        child_task.handle().promise().continuation = h;
                        child_task.handle().promise().parent_propagator = &h.promise();
                        return child_task.handle();
                    }

                    auto await_resume() -> ChildT
                    {
                        auto res = move(child_task.result());
                        if constexpr (!is_void_v<ChildT>)
                        {
                            return move(res.value());
                        }
                    }
                };

                return child_task_awaiter{move(child)};
            }

            // Await transform for unexpected<Err>
            template <typename Err>
            auto await_transform(unexpected<Err> unexp)
            {
                struct unexp_awaiter
                {
                    unexpected<Err> error;

                    constexpr auto await_ready() const noexcept -> bool
                    {
                        return false;
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        auto* parent = h.promise().parent_propagator;
                        h.promise().propagate_error(static_cast<job_error>(error.value));
                        auto next = h.promise().get_continuation();
                        h.destroy();
                        if (parent != nullptr)
                        {
                            parent->destroy_frame();
                        }
                        return next;
                    }

                    auto await_resume() -> void
                    {
                    }
                };

                return unexp_awaiter{unexp};
            }

            // Await transform for expected<Val, Err>
            template <typename Val, typename Err>
            auto await_transform(expected<Val, Err>&& exp)
            {
                struct exp_awaiter
                {
                    expected<Val, Err> value;

                    auto await_ready() const noexcept -> bool
                    {
                        return value.has_value();
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        auto* parent = h.promise().parent_propagator;
                        h.promise().propagate_error(static_cast<job_error>(value.error()));
                        auto next = h.promise().get_continuation();
                        h.destroy();
                        if (parent != nullptr)
                        {
                            parent->destroy_frame();
                        }
                        return next;
                    }

                    auto await_resume() -> Val
                    {
                        if constexpr (!is_void_v<Val>)
                        {
                            return move(value.value());
                        }
                    }
                };

                return exp_awaiter{move(exp)};
            }

            // Fallthrough for generic awaitables
            template <typename Awaitable>
            auto await_transform(Awaitable&& awaitable) -> decltype(auto)
            {
                return forward<Awaitable>(awaitable);
            }
        };

        task() noexcept = default;

        explicit task(coroutine_handle<promise_type> handle) noexcept : _handle{handle}
        {
            if (_handle)
            {
                _handle.promise().owner = this;
            }
        }

        ~task()
        {
            if (_handle)
            {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        task(task&& other) noexcept
            : _handle{other._handle}, _saved_result{move(other._saved_result)}, _is_completed{other._is_completed}
        {
            other._handle = nullptr;
            other._is_completed = false;
            if (_handle)
            {
                _handle.promise().owner = this;
            }
        }

        task& operator=(task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                {
                    _handle.destroy();
                }
                _handle = other._handle;
                _saved_result = move(other._saved_result);
                _is_completed = other._is_completed;
                other._handle = nullptr;
                other._is_completed = false;
                if (_handle)
                {
                    _handle.promise().owner = this;
                }
            }
            return *this;
        }

        [[nodiscard]] auto is_ready() const noexcept -> bool
        {
            if (_is_completed)
            {
                return true;
            }
            return _handle && _handle.done();
        }

        auto resume() -> bool
        {
            if (is_ready())
            {
                return true;
            }
            if (_handle)
            {
                _handle.resume();
            }
            return is_ready();
        }

        [[nodiscard]] auto has_value() const -> bool
        {
            return result().has_value();
        }

        [[nodiscard]] auto error() const -> E
        {
            return has_value() ? static_cast<E>(0) : result().error();
        }

        [[nodiscard]] auto result() -> result_type&
        {
            if (_is_completed)
            {
                return _saved_result;
            }
            return _handle.promise().result;
        }

        [[nodiscard]] auto result() const -> const result_type&
        {
            if (_is_completed)
            {
                return _saved_result;
            }
            return _handle.promise().result;
        }

        [[nodiscard]] auto value() -> T&
        {
            return result().value();
        }

        [[nodiscard]] auto value() const -> const T&
        {
            return result().value();
        }

        [[nodiscard]] auto handle() const noexcept -> coroutine_handle<promise_type>
        {
            return _handle;
        }

        auto operator co_await() &&
        {
            return task_awaiter<T, E>{move(*this)};
        }

      private:
        template <typename, typename>
        friend class task;

        coroutine_handle<promise_type> _handle{nullptr};
        result_type _saved_result{unexpected<E>{E::none}};
        bool _is_completed{false};
    };

    // =========================================================================
    // Fallible task without value: task<void, E> (where E != void)
    // =========================================================================
    template <typename E>
    class [[nodiscard]] task<void, E>
    {
      public:
        using value_type = void;
        using error_type = E;
        using result_type = expected<void, E>;

        struct promise_type : detail::promise_allocator_base, detail::error_propagator
        {
            task* owner{nullptr};
            result_type result{};
            coroutine_handle<> continuation{nullptr};
            detail::error_propagator* parent_propagator{nullptr};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> suspend_always
            {
                return {};
            }

            auto final_suspend() noexcept -> detail::task_final_awaiter<promise_type>
            {
                return {};
            }

            auto return_void() noexcept -> void
            {
                result = result_type{};
            }

            auto unhandled_exception() noexcept -> void
            {
                TEMPEST_ASSERT(false);
            }

            auto propagate_error(job_error err) -> void override
            {
                result = unexpected<E>(static_cast<E>(err));
                if (owner != nullptr)
                {
                    owner->_saved_result = result;
                    owner->_is_completed = true;
                    owner->_handle = nullptr;
                }
                if (parent_propagator != nullptr)
                {
                    parent_propagator->propagate_error(err);
                }
            }

            auto get_continuation() -> coroutine_handle<> override
            {
                if (parent_propagator != nullptr)
                {
                    return parent_propagator->get_continuation();
                }
                return continuation ? continuation : noop_coroutine();
            }

            auto destroy_frame() -> void override
            {
                auto* parent = parent_propagator;
                auto h = coroutine_handle<promise_type>::from_promise(*this);
                h.destroy();
                if (parent != nullptr)
                {
                    parent->destroy_frame();
                }
            }

            // Await transform for child fallible task
            template <typename ChildT, typename ChildE>
            auto await_transform(task<ChildT, ChildE>&& child)
            {
                struct child_task_awaiter
                {
                    task<ChildT, ChildE> child_task;

                    auto await_ready() const noexcept -> bool
                    {
                        return child_task.is_ready();
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        child_task.handle().promise().continuation = h;
                        child_task.handle().promise().parent_propagator = &h.promise();
                        return child_task.handle();
                    }

                    auto await_resume() -> ChildT
                    {
                        auto res = move(child_task.result());
                        if constexpr (!is_void_v<ChildT>)
                        {
                            return move(res.value());
                        }
                    }
                };

                return child_task_awaiter{move(child)};
            }

            // Await transform for unexpected<Err>
            template <typename Err>
            auto await_transform(unexpected<Err> unexp)
            {
                struct unexp_awaiter
                {
                    unexpected<Err> error;

                    constexpr auto await_ready() const noexcept -> bool
                    {
                        return false;
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        auto* parent = h.promise().parent_propagator;
                        h.promise().propagate_error(static_cast<job_error>(error.value));
                        auto next = h.promise().get_continuation();
                        h.destroy();
                        if (parent != nullptr)
                        {
                            parent->destroy_frame();
                        }
                        return next;
                    }

                    auto await_resume() -> void
                    {
                    }
                };

                return unexp_awaiter{unexp};
            }

            // Await transform for expected<Val, Err>
            template <typename Val, typename Err>
            auto await_transform(expected<Val, Err>&& exp)
            {
                struct exp_awaiter
                {
                    expected<Val, Err> value;

                    auto await_ready() const noexcept -> bool
                    {
                        return value.has_value();
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        auto* parent = h.promise().parent_propagator;
                        h.promise().propagate_error(static_cast<job_error>(value.error()));
                        auto next = h.promise().get_continuation();
                        h.destroy();
                        if (parent != nullptr)
                        {
                            parent->destroy_frame();
                        }
                        return next;
                    }

                    auto await_resume() -> Val
                    {
                        if constexpr (!is_void_v<Val>)
                        {
                            return move(value.value());
                        }
                    }
                };

                return exp_awaiter{move(exp)};
            }

            template <typename Awaitable>
            auto await_transform(Awaitable&& awaitable) -> decltype(auto)
            {
                return forward<Awaitable>(awaitable);
            }
        };

        task() noexcept = default;

        explicit task(coroutine_handle<promise_type> handle) noexcept : _handle{handle}
        {
            if (_handle)
            {
                _handle.promise().owner = this;
            }
        }

        ~task()
        {
            if (_handle)
            {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        task(task&& other) noexcept
            : _handle{other._handle}, _saved_result{move(other._saved_result)}, _is_completed{other._is_completed}
        {
            other._handle = nullptr;
            other._is_completed = false;
            if (_handle)
            {
                _handle.promise().owner = this;
            }
        }

        task& operator=(task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                {
                    _handle.destroy();
                }
                _handle = other._handle;
                _saved_result = move(other._saved_result);
                _is_completed = other._is_completed;
                other._handle = nullptr;
                other._is_completed = false;
                if (_handle)
                {
                    _handle.promise().owner = this;
                }
            }
            return *this;
        }

        [[nodiscard]] auto is_ready() const noexcept -> bool
        {
            if (_is_completed)
            {
                return true;
            }
            return _handle && _handle.done();
        }

        auto resume() -> bool
        {
            if (is_ready())
            {
                return true;
            }
            if (_handle)
            {
                _handle.resume();
            }
            return is_ready();
        }

        [[nodiscard]] auto has_value() const -> bool
        {
            return result().has_value();
        }

        [[nodiscard]] auto error() const -> E
        {
            return has_value() ? static_cast<E>(0) : result().error();
        }

        auto value() -> void
        {
            result().value();
        }

        [[nodiscard]] auto result() -> result_type&
        {
            if (_is_completed)
            {
                return _saved_result;
            }
            return _handle.promise().result;
        }

        [[nodiscard]] auto result() const -> const result_type&
        {
            if (_is_completed)
            {
                return _saved_result;
            }
            return _handle.promise().result;
        }

        [[nodiscard]] auto handle() const noexcept -> coroutine_handle<promise_type>
        {
            return _handle;
        }

        auto operator co_await() &&
        {
            return task_awaiter<void, E>{move(*this)};
        }

      private:
        template <typename, typename>
        friend class task;

        coroutine_handle<promise_type> _handle{nullptr};
        result_type _saved_result{unexpected<E>{E::none}};
        bool _is_completed{false};
    };

    // =========================================================================
    // Non-fallible task with value: task<T, void> (where T != void)
    // =========================================================================
    template <typename T>
    class [[nodiscard]] task<T, void>
    {
      public:
        using value_type = T;
        using error_type = void;

        struct promise_type : detail::promise_allocator_base
        {
            optional<T> result{nullopt};
            coroutine_handle<> continuation{nullptr};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> suspend_always
            {
                return {};
            }

            auto final_suspend() noexcept -> detail::task_final_awaiter<promise_type>
            {
                return {};
            }

            auto return_value(T value) noexcept -> void
            {
                result = move(value);
            }

            auto unhandled_exception() noexcept -> void
            {
                TEMPEST_ASSERT(false);
            }
        };

        task() noexcept = default;

        explicit task(coroutine_handle<promise_type> handle) noexcept : _handle{handle}
        {
        }

        ~task()
        {
            if (_handle)
            {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        task(task&& other) noexcept : _handle{other._handle}
        {
            other._handle = nullptr;
        }

        task& operator=(task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                {
                    _handle.destroy();
                }
                _handle = other._handle;
                other._handle = nullptr;
            }
            return *this;
        }

        [[nodiscard]] auto is_ready() const noexcept -> bool
        {
            return _handle && _handle.done();
        }

        auto resume() -> bool
        {
            if (is_ready())
            {
                return true;
            }
            if (_handle)
            {
                _handle.resume();
            }
            return is_ready();
        }

        [[nodiscard]] auto value() & -> T&
        {
            return *_handle.promise().result;
        }

        [[nodiscard]] auto value() const& -> const T&
        {
            return *_handle.promise().result;
        }

        [[nodiscard]] auto value() && -> T&&
        {
            return move(*_handle.promise().result);
        }

        [[nodiscard]] auto handle() const noexcept -> coroutine_handle<promise_type>
        {
            return _handle;
        }

        auto operator co_await() &&
        {
            return task_awaiter<T, void>{move(*this)};
        }

      private:
        coroutine_handle<promise_type> _handle{nullptr};
    };

    // =========================================================================
    // Non-fallible task without value: task<void, void>
    // =========================================================================
    template <>
    class [[nodiscard]] task<void, void>
    {
      public:
        using value_type = void;
        using error_type = void;

        struct promise_type : detail::promise_allocator_base
        {
            coroutine_handle<> continuation{nullptr};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> suspend_always
            {
                return {};
            }

            auto final_suspend() noexcept -> detail::task_final_awaiter<promise_type>
            {
                return {};
            }

            auto return_void() noexcept -> void
            {
            }

            auto unhandled_exception() noexcept -> void
            {
                TEMPEST_ASSERT(false);
            }
        };

        task() noexcept = default;

        explicit task(coroutine_handle<promise_type> handle) noexcept : _handle{handle}
        {
        }

        ~task()
        {
            if (_handle)
            {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        task(const task&) = delete;
        task& operator=(const task&) = delete;

        task(task&& other) noexcept : _handle{other._handle}
        {
            other._handle = nullptr;
        }

        task& operator=(task&& other) noexcept
        {
            if (this != &other)
            {
                if (_handle)
                {
                    _handle.destroy();
                }
                _handle = other._handle;
                other._handle = nullptr;
            }
            return *this;
        }

        [[nodiscard]] auto is_ready() const noexcept -> bool
        {
            return _handle && _handle.done();
        }

        auto resume() -> bool
        {
            if (is_ready())
            {
                return true;
            }
            if (_handle)
            {
                _handle.resume();
            }
            return is_ready();
        }

        auto value() const noexcept -> void
        {
        }

        [[nodiscard]] auto handle() const noexcept -> coroutine_handle<promise_type>
        {
            return _handle;
        }

        auto operator co_await() &&
        {
            return task_awaiter<void, void>{move(*this)};
        }

      private:
        coroutine_handle<promise_type> _handle{nullptr};
    };
} // namespace tempest::job

#endif // tempest_job_task_hpp
