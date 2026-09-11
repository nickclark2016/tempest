#ifndef tempest_job_task_hpp
#define tempest_job_task_hpp

#include <tempest/api.hpp>
#include <tempest/assert.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/job/allocator.hpp>
#include <tempest/job/context.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/profiler/types.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    class job_system;

    inline atomic<uint64_t> g_next_coroutine_id{1};

    template <typename T = void, typename E = job_error>
    class [[nodiscard]] task;

    namespace detail
    {
        struct alignas(16) coroutine_frame_header
        {
            job_allocator* owner{nullptr};
            uint32_t frame_size{0};
            uint16_t size_class{0};
            uint16_t is_heap{0};
        };
        static_assert(sizeof(coroutine_frame_header) == 16);

        template <typename T>
        concept complete_type = requires { sizeof(T); };

        template <typename T>
        constexpr auto get_allocator_from_arg([[maybe_unused]] T&& arg) noexcept -> job_allocator*
        {
            using CleanT = remove_cvref_t<T>;
            if constexpr (is_same_v<CleanT, job_allocator>)
            {
                return const_cast<job_allocator*>(&arg);
            }
            else if constexpr (is_same_v<CleanT, job_context>)
            {
                return arg.allocator.get();
            }
            else if constexpr (complete_type<CleanT>)
            {
                if constexpr (requires { { arg.get_dispatch_allocator() } -> same_as<job_allocator&>; })
                {
                    return &arg.get_dispatch_allocator();
                }
                else if constexpr (requires { { arg.get_job_allocator() } -> same_as<job_allocator&>; })
                {
                    return &arg.get_job_allocator();
                }
                else
                {
                    return nullptr;
                }
            }
            else
            {
                return nullptr;
            }
        }

        template <typename... Args>
        constexpr auto find_allocator(Args&&... args) noexcept -> job_allocator*
        {
            job_allocator* result = nullptr;
            auto check = [&result](job_allocator* alloc) {
                if (result == nullptr && alloc != nullptr)
                {
                    result = alloc;
                }
            };
            (check(get_allocator_from_arg(forward<Args>(args))), ...);
            return result;
        }

        struct promise_allocator_base
        {
            template <typename... Args>
            static auto operator new(size_t size, Args&&... args) -> void*
            {
                constexpr auto header_size = sizeof(coroutine_frame_header);
                const auto total_size = size + header_size;
                auto* alloc = find_allocator(forward<Args>(args)...);

                if (alloc != nullptr)
                {
                    auto* raw = static_cast<byte*>(alloc->allocate(total_size));
                    auto* header = reinterpret_cast<coroutine_frame_header*>(raw);
                    header->owner = alloc;
                    header->frame_size = static_cast<uint32_t>(size);
                    const auto sc = get_size_class(total_size);
                    header->size_class = (sc >= 0) ? static_cast<uint16_t>(sc) : 0xFFFF;
                    header->is_heap = (sc < 0) ? 1 : 0;
                    return raw + header_size;
                }

                auto* raw = static_cast<byte*>(tempest::aligned_alloc(total_size, 16));
                auto* header = reinterpret_cast<coroutine_frame_header*>(raw);
                header->owner = nullptr;
                header->frame_size = static_cast<uint32_t>(size);
                header->size_class = 0xFFFF;
                header->is_heap = 1;
                return raw + header_size;
            }

            static auto operator new(size_t size) -> void*
            {
                return operator new<>(size);
            }

            static auto operator delete(void* ptr, size_t size) noexcept -> void
            {
                if (ptr == nullptr)
                {
                    return;
                }
                constexpr auto header_size = sizeof(coroutine_frame_header);
                const auto total_size = size + header_size;
                auto* header = reinterpret_cast<coroutine_frame_header*>(static_cast<byte*>(ptr) - header_size);

                if (header->owner != nullptr)
                {
                    header->owner->deallocate(header, total_size);
                }
                else
                {
                    tempest::aligned_free(header);
                }
            }
        };

        struct detached_task
        {
            struct promise_type : promise_allocator_base
            {
                auto get_return_object() noexcept -> detached_task
                {
                    return detached_task{coroutine_handle<promise_type>::from_promise(*this)};
                }
                auto initial_suspend() noexcept -> suspend_always
                {
                    return {};
                }
                auto final_suspend() noexcept -> suspend_never
                {
                    return {};
                }
                auto return_void() noexcept -> void
                {
                }
                auto unhandled_exception() noexcept -> void
                {
                }
            };

            coroutine_handle<promise_type> handle{nullptr};
        };

        template <typename T>
        struct is_task_helper : false_type
        {
        };

        template <typename T, typename E>
        struct is_task_helper<task<T, E>> : true_type
        {
        };

        template <typename T>
        inline constexpr bool is_task_v = is_task_helper<remove_cvref_t<T>>::value;

        template <typename T>
        struct task_traits
        {
            using value_type = void;
            using error_type = void;
        };

        template <typename T, typename E>
        struct task_traits<task<T, E>>
        {
            using value_type = T;
            using error_type = E;
        };

        template <typename PrevVal, typename F>
        struct continuation_invoke_result
        {
            using type = invoke_result_t<decay_t<F>, PrevVal>;
        };

        template <typename F>
        struct continuation_invoke_result<void, F>
        {
            using type = invoke_result_t<decay_t<F>>;
        };

        template <typename PrevTask, typename F>
        struct continuation_result
        {
            using prev_val_t = typename PrevTask::value_type;
            using prev_err_t = typename PrevTask::error_type;

            using raw_invoke_t = typename continuation_invoke_result<prev_val_t, F>::type;

            using result_val_t = conditional_t<
                is_task_v<raw_invoke_t>,
                typename task_traits<raw_invoke_t>::value_type,
                raw_invoke_t>;

            using child_err_t = conditional_t<
                is_task_v<raw_invoke_t>,
                typename task_traits<raw_invoke_t>::error_type,
                void>;

            using result_err_t = conditional_t<
                !is_void_v<prev_err_t>,
                prev_err_t,
                child_err_t>;

            using type = task<result_val_t, result_err_t>;
        };

        template <typename PrevTask, typename F>
        using continuation_result_t = typename continuation_result<PrevTask, F>::type;

        template <typename Awaiter>
        concept has_suspend_reason = requires(const Awaiter& a) {
            { a.suspend_reason_tag() } -> same_as<profiler::suspend_reason>;
        };

        template <typename Awaiter>
        constexpr auto get_suspend_reason(const Awaiter& a) noexcept -> profiler::suspend_reason
        {
            if constexpr (has_suspend_reason<Awaiter>)
            {
                return a.suspend_reason_tag();
            }
            else
            {
                return profiler::suspend_reason::yield;
            }
        }

        template <typename T>
        decltype(auto) get_underlying_awaiter(T&& expr)
        {
            if constexpr (requires { tempest::forward<T>(expr).operator co_await(); })
            {
                return tempest::forward<T>(expr).operator co_await();
            }
            else if constexpr (requires { operator co_await(tempest::forward<T>(expr)); })
            {
                return operator co_await(tempest::forward<T>(expr));
            }
            else
            {
                return tempest::forward<T>(expr);
            }
        }

        template <typename Promise, typename Awaiter>
        struct profiled_awaiter;

        template <typename T>
        struct is_profiled_awaiter : false_type
        {
        };

        template <typename P, typename A>
        struct is_profiled_awaiter<profiled_awaiter<P, A>> : true_type
        {
        };

        template <typename T>
        inline constexpr bool is_profiled_awaiter_v = is_profiled_awaiter<remove_cvref_t<T>>::value;

        template <typename Promise, typename Awaiter>
        struct profiled_awaiter
        {
            Promise& promise;
            Awaiter awaiter;
            profiler::suspend_reason reason{profiler::suspend_reason::yield};
            bool did_suspend{false};

            auto await_ready() noexcept(noexcept(awaiter.await_ready())) -> decltype(auto)
            {
                return awaiter.await_ready();
            }

            template <typename Handle>
            auto await_suspend(Handle h) -> decltype(auto)
            {
                did_suspend = true;
                auto* ctx = profiler::thread_profiler_context::get_current_thread_context();
                if (ctx != nullptr && ctx->get_session().is_enabled())
                {
                    ctx->end_coroutine_slice(reason);
                }
                return awaiter.await_suspend(h);
            }

            auto await_resume() -> decltype(auto)
            {
                if (did_suspend)
                {
                    auto* ctx = profiler::thread_profiler_context::get_current_thread_context();
                    if (ctx != nullptr && ctx->get_session().is_enabled())
                    {
                        promise.current_slice_index++;
                        ctx->begin_coroutine_slice(promise.coroutine_id, promise.current_slice_index, promise.name);
                        auto h = coroutine_handle<Promise>::from_promise(promise);
                        auto* header = reinterpret_cast<const coroutine_frame_header*>(
                            static_cast<const byte*>(h.address()) - sizeof(coroutine_frame_header));
                        ctx->add_metric("frame_bytes", static_cast<double>(header->frame_size), profiler::metric_unit::bytes);
                        ctx->add_metric("is_heap", header->is_heap ? 1.0 : 0.0, profiler::metric_unit::raw);
                    }
                }
                return awaiter.await_resume();
            }
        };

        template <typename Promise>
        struct task_initial_awaiter
        {
            Promise& promise;

            [[nodiscard]] constexpr auto await_ready() const noexcept -> bool
            {
                return false;
            }

            auto await_suspend(coroutine_handle<Promise>) noexcept -> void
            {
            }

            auto await_resume() const noexcept -> void
            {
                auto* ctx = profiler::thread_profiler_context::get_current_thread_context();
                if (ctx != nullptr && ctx->get_session().is_enabled())
                {
                    ctx->begin_coroutine_slice(promise.coroutine_id, promise.current_slice_index, promise.name);
                    auto h = coroutine_handle<Promise>::from_promise(promise);
                    auto* header = reinterpret_cast<const coroutine_frame_header*>(
                        static_cast<const byte*>(h.address()) - sizeof(coroutine_frame_header));
                    ctx->add_metric("frame_bytes", static_cast<double>(header->frame_size), profiler::metric_unit::bytes);
                    ctx->add_metric("is_heap", header->is_heap ? 1.0 : 0.0, profiler::metric_unit::raw);
                }
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
                auto* ctx = profiler::thread_profiler_context::get_current_thread_context();
                if (ctx != nullptr && ctx->get_session().is_enabled())
                {
                    ctx->end_coroutine_slice(profiler::suspend_reason::completed);
                }

                if constexpr (requires { h.promise().result.has_value(); h.promise().parent_propagator; })
                {
                    if (!h.promise().result.has_value() && h.promise().parent_propagator != nullptr)
                    {
                        auto* parent = h.promise().parent_propagator;
                        parent->propagate_error(static_cast<job_error>(h.promise().result.error()));
                        auto cont = parent->get_continuation();
                        parent->destroy_frame();
                        return cont ? cont : noop_coroutine();
                    }
                }
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
            if constexpr (requires { awaited_task.handle().promise().parent_propagator = &h.promise(); })
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
            uint64_t coroutine_id{g_next_coroutine_id.fetch_add(1, memory_order::relaxed)};
            uint32_t current_slice_index{0};
            string_view name{"task"};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> detail::task_initial_awaiter<promise_type>
            {
                return detail::task_initial_awaiter<promise_type>{*this};
            }

            auto final_suspend() noexcept -> detail::task_final_awaiter<promise_type>
            {
                return {};
            }

            auto return_value(T value) noexcept -> void
            {
                result = move(value);
            }

            template <typename U>
                requires is_same_v<remove_cvref_t<U>, result_type> && (!is_same_v<remove_cvref_t<U>, T>)
            auto return_value(U&& res) noexcept -> void
            {
                result = forward<U>(res);
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
                requires (!is_void_v<ChildE>)
            auto await_transform(task<ChildT, ChildE>&& child)
            {
                struct child_task_awaiter
                {
                    task<ChildT, ChildE> child_task;

                    auto await_ready() const noexcept -> bool
                    {
                        return child_task.is_ready() && child_task.has_value();
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        if (child_task.is_ready())
                        {
                            auto err = child_task.error();
                            auto* parent = h.promise().parent_propagator;
                            h.promise().propagate_error(static_cast<job_error>(err));
                            auto next = h.promise().get_continuation();
                            h.destroy();
                            if (parent != nullptr)
                            {
                                parent->destroy_frame();
                            }
                            return next ? next : noop_coroutine();
                        }
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

                return detail::profiled_awaiter<promise_type, child_task_awaiter>{
                    *this, child_task_awaiter{move(child)}, profiler::suspend_reason::yield};
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

                return detail::profiled_awaiter<promise_type, unexp_awaiter>{
                    *this, unexp_awaiter{unexp}, profiler::suspend_reason::yield};
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

                return detail::profiled_awaiter<promise_type, exp_awaiter>{
                    *this, exp_awaiter{move(exp)}, profiler::suspend_reason::yield};
            }

            // Fallthrough for generic awaitables
            template <typename Awaitable>
            auto await_transform(Awaitable&& awaitable)
            {
                if constexpr (detail::is_profiled_awaiter_v<Awaitable>)
                {
                    return forward<Awaitable>(awaitable);
                }
                else
                {
                    auto actual = detail::get_underlying_awaiter(forward<Awaitable>(awaitable));
                    auto reason = detail::get_suspend_reason(actual);
                    return detail::profiled_awaiter<promise_type, decltype(actual)>{
                        *this, move(actual), reason};
                }
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

        [[nodiscard]] auto coroutine_id() const noexcept -> uint64_t
        {
            return _handle ? _handle.promise().coroutine_id : 0;
        }

        auto operator co_await() &&
        {
            return task_awaiter<T, E>{move(*this)};
        }

        template <typename F>
        auto then(F&& func) && -> detail::continuation_result_t<task, F>;

        template <typename F>
        auto then(F&& func) & -> detail::continuation_result_t<task, F>;

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
            uint64_t coroutine_id{g_next_coroutine_id.fetch_add(1, memory_order::relaxed)};
            uint32_t current_slice_index{0};
            string_view name{"task"};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> detail::task_initial_awaiter<promise_type>
            {
                return detail::task_initial_awaiter<promise_type>{*this};
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
                requires (!is_void_v<ChildE>)
            auto await_transform(task<ChildT, ChildE>&& child)
            {
                struct child_task_awaiter
                {
                    task<ChildT, ChildE> child_task;

                    auto await_ready() const noexcept -> bool
                    {
                        return child_task.is_ready() && child_task.has_value();
                    }

                    auto await_suspend(coroutine_handle<promise_type> h) noexcept -> coroutine_handle<>
                    {
                        if (child_task.is_ready())
                        {
                            auto err = child_task.error();
                            auto* parent = h.promise().parent_propagator;
                            h.promise().propagate_error(static_cast<job_error>(err));
                            auto next = h.promise().get_continuation();
                            h.destroy();
                            if (parent != nullptr)
                            {
                                parent->destroy_frame();
                            }
                            return next ? next : noop_coroutine();
                        }
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

                return detail::profiled_awaiter<promise_type, child_task_awaiter>{
                    *this, child_task_awaiter{move(child)}, profiler::suspend_reason::yield};
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

                return detail::profiled_awaiter<promise_type, unexp_awaiter>{
                    *this, unexp_awaiter{unexp}, profiler::suspend_reason::yield};
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

                return detail::profiled_awaiter<promise_type, exp_awaiter>{
                    *this, exp_awaiter{move(exp)}, profiler::suspend_reason::yield};
            }

            template <typename Awaitable>
            auto await_transform(Awaitable&& awaitable)
            {
                if constexpr (detail::is_profiled_awaiter_v<Awaitable>)
                {
                    return forward<Awaitable>(awaitable);
                }
                else
                {
                    auto actual = detail::get_underlying_awaiter(forward<Awaitable>(awaitable));
                    auto reason = detail::get_suspend_reason(actual);
                    return detail::profiled_awaiter<promise_type, decltype(actual)>{
                        *this, move(actual), reason};
                }
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

        [[nodiscard]] auto coroutine_id() const noexcept -> uint64_t
        {
            return _handle ? _handle.promise().coroutine_id : 0;
        }

        auto operator co_await() &&
        {
            return task_awaiter<void, E>{move(*this)};
        }

        template <typename F>
        auto then(F&& func) && -> detail::continuation_result_t<task, F>;

        template <typename F>
        auto then(F&& func) & -> detail::continuation_result_t<task, F>;

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
            uint64_t coroutine_id{g_next_coroutine_id.fetch_add(1, memory_order::relaxed)};
            uint32_t current_slice_index{0};
            string_view name{"task"};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> detail::task_initial_awaiter<promise_type>
            {
                return detail::task_initial_awaiter<promise_type>{*this};
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

            template <typename Awaitable>
            auto await_transform(Awaitable&& awaitable)
            {
                if constexpr (detail::is_profiled_awaiter_v<Awaitable>)
                {
                    return forward<Awaitable>(awaitable);
                }
                else
                {
                    auto actual = detail::get_underlying_awaiter(forward<Awaitable>(awaitable));
                    auto reason = detail::get_suspend_reason(actual);
                    return detail::profiled_awaiter<promise_type, decltype(actual)>{
                        *this, move(actual), reason};
                }
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

        [[nodiscard]] auto coroutine_id() const noexcept -> uint64_t
        {
            return _handle ? _handle.promise().coroutine_id : 0;
        }

        auto operator co_await() &&
        {
            return task_awaiter<T, void>{move(*this)};
        }

        template <typename F>
        auto then(F&& func) && -> detail::continuation_result_t<task, F>;

        template <typename F>
        auto then(F&& func) & -> detail::continuation_result_t<task, F>;

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
            uint64_t coroutine_id{g_next_coroutine_id.fetch_add(1, memory_order::relaxed)};
            uint32_t current_slice_index{0};
            string_view name{"task"};

            auto get_return_object() noexcept -> task
            {
                return task{coroutine_handle<promise_type>::from_promise(*this)};
            }

            auto initial_suspend() noexcept -> detail::task_initial_awaiter<promise_type>
            {
                return detail::task_initial_awaiter<promise_type>{*this};
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

            template <typename Awaitable>
            auto await_transform(Awaitable&& awaitable)
            {
                if constexpr (detail::is_profiled_awaiter_v<Awaitable>)
                {
                    return forward<Awaitable>(awaitable);
                }
                else
                {
                    auto actual = detail::get_underlying_awaiter(forward<Awaitable>(awaitable));
                    auto reason = detail::get_suspend_reason(actual);
                    return detail::profiled_awaiter<promise_type, decltype(actual)>{
                        *this, move(actual), reason};
                }
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

        [[nodiscard]] auto coroutine_id() const noexcept -> uint64_t
        {
            return _handle ? _handle.promise().coroutine_id : 0;
        }

        auto operator co_await() &&
        {
            return task_awaiter<void, void>{move(*this)};
        }

        template <typename F>
        auto then(F&& func) && -> detail::continuation_result_t<task, F>;

        template <typename F>
        auto then(F&& func) & -> detail::continuation_result_t<task, F>;

      private:
        coroutine_handle<promise_type> _handle{nullptr};
    };

    namespace detail
    {
        template <typename ResultTask, typename PrevTask, typename F>
        auto make_continuation(PrevTask prev, F func) -> ResultTask
        {
            using prev_val_t = typename PrevTask::value_type;
            using raw_invoke_t = typename continuation_invoke_result<prev_val_t, F>::type;

            if constexpr (is_void_v<prev_val_t>)
            {
                co_await move(prev);
                if constexpr (is_task_v<raw_invoke_t>)
                {
                    using inner_val_t = typename task_traits<raw_invoke_t>::value_type;
                    if constexpr (is_void_v<inner_val_t>)
                    {
                        co_await func();
                        co_return;
                    }
                    else
                    {
                        co_return co_await func();
                    }
                }
                else if constexpr (is_void_v<raw_invoke_t>)
                {
                    func();
                    co_return;
                }
                else
                {
                    co_return func();
                }
            }
            else
            {
                auto val = co_await move(prev);
                if constexpr (is_task_v<raw_invoke_t>)
                {
                    using inner_val_t = typename task_traits<raw_invoke_t>::value_type;
                    if constexpr (is_void_v<inner_val_t>)
                    {
                        co_await func(move(val));
                        co_return;
                    }
                    else
                    {
                        co_return co_await func(move(val));
                    }
                }
                else if constexpr (is_void_v<raw_invoke_t>)
                {
                    func(move(val));
                    co_return;
                }
                else
                {
                    co_return func(move(val));
                }
            }
        }
    } // namespace detail

    template <typename T, typename E>
    template <typename F>
    auto task<T, E>::then(F&& func) && -> detail::continuation_result_t<task<T, E>, F>
    {
        return detail::make_continuation<detail::continuation_result_t<task<T, E>, F>>(
            move(*this), forward<F>(func));
    }

    template <typename T, typename E>
    template <typename F>
    auto task<T, E>::then(F&& func) & -> detail::continuation_result_t<task<T, E>, F>
    {
        return move(*this).then(forward<F>(func));
    }

    template <typename E>
    template <typename F>
    auto task<void, E>::then(F&& func) && -> detail::continuation_result_t<task<void, E>, F>
    {
        return detail::make_continuation<detail::continuation_result_t<task<void, E>, F>>(
            move(*this), forward<F>(func));
    }

    template <typename E>
    template <typename F>
    auto task<void, E>::then(F&& func) & -> detail::continuation_result_t<task<void, E>, F>
    {
        return move(*this).then(forward<F>(func));
    }

    template <typename T>
    template <typename F>
    auto task<T, void>::then(F&& func) && -> detail::continuation_result_t<task<T, void>, F>
    {
        return detail::make_continuation<detail::continuation_result_t<task<T, void>, F>>(
            move(*this), forward<F>(func));
    }

    template <typename T>
    template <typename F>
    auto task<T, void>::then(F&& func) & -> detail::continuation_result_t<task<T, void>, F>
    {
        return move(*this).then(forward<F>(func));
    }

    template <typename F>
    inline auto task<void, void>::then(F&& func) && -> detail::continuation_result_t<task<void, void>, F>
    {
        return detail::make_continuation<detail::continuation_result_t<task<void, void>, F>>(
            move(*this), forward<F>(func));
    }

    template <typename F>
    inline auto task<void, void>::then(F&& func) & -> detail::continuation_result_t<task<void, void>, F>
    {
        return move(*this).then(forward<F>(func));
    }
} // namespace tempest::job

#endif // tempest_job_task_hpp
