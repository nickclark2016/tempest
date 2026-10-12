#ifndef tempest_job_job_system_hpp
#define tempest_job_job_system_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/allocator.hpp>
#include <tempest/job/async_event.hpp>
#include <tempest/job/context.hpp>
#include <tempest/job/parallel_for.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/task_graph.hpp>
#include <tempest/job/topology.hpp>
#include <tempest/job/types.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/optional.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/source_location.hpp>
#include <tempest/string_view.hpp>
#include <tempest/tuple.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    struct job_system_config
    {
        static constexpr uint32_t default_starvation_quantum = 16;

        optional<uint32_t> performance_worker_count = nullopt;
        optional<uint32_t> efficiency_worker_count = nullopt;
        bool enable_work_stealing = true;
        bool enable_core_pinning = true;
        uint32_t starvation_quantum = default_starvation_quantum;
        optional<cpu_topology> topology = nullopt;
    };

    class TEMPEST_API job_system
    {
      public:
        struct queue_item
        {
            coroutine_handle<> handle = nullptr;
            task_priority priority = task_priority::normal;
            core_class affinity = core_class::any;
        };

        job_system(logger& log, profiler::profiler_session& profiler, job_system_config config = {});
        job_system(const job_system&) = delete;
        job_system(job_system&&) noexcept = delete;
        ~job_system();

        auto operator=(const job_system&) -> job_system& = delete;
        auto operator=(job_system&&) noexcept -> job_system& = delete;

        [[nodiscard]] auto get_logger() noexcept -> logger&
        {
            return _logger;
        }

        [[nodiscard]] auto get_profiler() noexcept -> profiler::profiler_session&
        {
            return _profiler;
        }

        [[nodiscard]] auto get_config() const noexcept -> const job_system_config&
        {
            return _config;
        }

        [[nodiscard]] auto get_topology() const noexcept -> const cpu_topology&;

        [[nodiscard]] auto worker_count() const noexcept -> uint32_t;
        [[nodiscard]] auto performance_worker_count() const noexcept -> uint32_t;
        [[nodiscard]] auto efficiency_worker_count() const noexcept -> uint32_t;

        [[nodiscard]] auto get_dispatch_allocator() noexcept -> job_allocator&;
        [[nodiscard]] auto allocate_frame(size_t size) -> void*;

        auto schedule(coroutine_handle<> handle, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) -> void;
        auto schedule(const job_context& ctx, coroutine_handle<> handle, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) -> void;

        template <typename T, typename E>
        auto schedule(task<T, E>& job, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any,
                      source_location loc = source_location::current()) -> void
        {
            if (job.get_profiler() == nullptr)
            {
                job.set_profiler(&_profiler);
            }
            if (job.name() == "task" && loc.function_name() != nullptr)
            {
                job.set_name(loc.function_name());
            }
            if (job.handle())
            {
                if (job.handle().promise().spawned_by_coroutine_id == 0)
                {
                    job.handle().promise().spawned_by_coroutine_id =
                        _profiler.get_or_register_thread().get_current_coroutine_id();
                }
                if (job.handle().promise().spawned_by_thread_id == 0)
                {
                    job.handle().promise().spawned_by_thread_id = tempest::this_thread::get_id().to_uint64();
                }
            }

            schedule(job.handle(), priority, affinity);
        }

        template <typename T, typename E>
        auto schedule(const job_context& ctx, task<T, E>& job, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any,
                      source_location loc = source_location::current()) -> void
        {
            if (job.get_profiler() == nullptr)
            {
                job.set_profiler(&_profiler);
            }
            if (job.name() == "task" && loc.function_name() != nullptr)
            {
                job.set_name(loc.function_name());
            }
            if (job.handle())
            {
                if (job.handle().promise().spawned_by_coroutine_id == 0)
                {
                    job.handle().promise().spawned_by_coroutine_id =
                        _profiler.get_or_register_thread().get_current_coroutine_id();
                }
                if (job.handle().promise().spawned_by_thread_id == 0)
                {
                    job.handle().promise().spawned_by_thread_id = tempest::this_thread::get_id().to_uint64();
                }
            }
            schedule(ctx, job.handle(), priority, affinity);
        }

        template <typename F>
        auto async(string_view name, task_priority priority, core_class affinity, F&& callable,
                   source_location loc = source_location::current())
        {
            using ReturnType = invoke_result_t<F>;
            const auto task_name = !name.empty()
                                       ? name
                                       : (loc.function_name() != nullptr ? string_view{loc.function_name()}
                                                                         : string_view{"async_task"});

            if constexpr (detail::is_task_v<ReturnType>)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
                auto runner = [](job_allocator&, job_system&, F func) -> ReturnType {
                    if constexpr (is_void_v<typename ReturnType::value_type>)
                    {
                        co_await func();
                        co_return;
                    }
                    else
                    {
                        co_return co_await func();
                    }
                };

                auto job = runner(get_dispatch_allocator(), *this, forward<F>(callable));
                job.set_name(task_name);
                job.set_source_location(loc);
                job.set_profiler(&_profiler);
                if (job.handle())
                {
                    if (job.handle().promise().spawned_by_coroutine_id == 0)
                    {
                        job.handle().promise().spawned_by_coroutine_id =
                            _profiler.get_or_register_thread().get_current_coroutine_id();
                    }
                    if (job.handle().promise().spawned_by_thread_id == 0)
                    {
                        job.handle().promise().spawned_by_thread_id = tempest::this_thread::get_id().to_uint64();
                    }
                }
                schedule(job.handle(), priority, affinity);
                return job;
            }
            else if constexpr (is_void_v<ReturnType>)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
                auto runner = [](job_allocator&, job_system&, F func) -> task<void> {
                    func();
                    co_return;
                };

                auto job = runner(get_dispatch_allocator(), *this, forward<F>(callable));
                job.set_name(task_name);
                job.set_source_location(loc);
                job.set_profiler(&_profiler);
                if (job.handle())
                {
                    if (job.handle().promise().spawned_by_coroutine_id == 0)
                    {
                        job.handle().promise().spawned_by_coroutine_id =
                            _profiler.get_or_register_thread().get_current_coroutine_id();
                    }
                    if (job.handle().promise().spawned_by_thread_id == 0)
                    {
                        job.handle().promise().spawned_by_thread_id = tempest::this_thread::get_id().to_uint64();
                    }
                }
                schedule(job.handle(), priority, affinity);
                return job;
            }
            else
            {
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
                auto runner = [](job_allocator&, job_system&, F func) -> task<ReturnType> { co_return func(); };

                auto job = runner(get_dispatch_allocator(), *this, forward<F>(callable));
                job.set_name(task_name);
                job.set_source_location(loc);
                job.set_profiler(&_profiler);
                if (job.handle())
                {
                    if (job.handle().promise().spawned_by_coroutine_id == 0)
                    {
                        job.handle().promise().spawned_by_coroutine_id =
                            _profiler.get_or_register_thread().get_current_coroutine_id();
                    }
                    if (job.handle().promise().spawned_by_thread_id == 0)
                    {
                        job.handle().promise().spawned_by_thread_id = tempest::this_thread::get_id().to_uint64();
                    }
                }
                schedule(job.handle(), priority, affinity);
                return job;
            }
        }

        template <typename F>
            requires(!is_convertible_v<F, string_view> && !is_same_v<remove_cvref_t<F>, task_priority>)
        auto async(F&& callable, source_location loc = source_location::current())
        {
            return async(string_view{}, task_priority::normal, core_class::any, forward<F>(callable), loc);
        }

        template <typename F>
        auto async(string_view name, F&& callable, source_location loc = source_location::current())
        {
            return async(name, task_priority::normal, core_class::any, forward<F>(callable), loc);
        }

        template <typename F>
        auto async(task_priority priority, core_class affinity, F&& callable,
                   source_location loc = source_location::current())
        {
            return async(string_view{}, priority, affinity, forward<F>(callable), loc);
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(range<size_t> rng, task_priority priority, F body,
                          source_location loc = source_location::current()) -> task<void>
        {
            auto impl = [](job_allocator&, job_system& sys, range<size_t> r, task_priority prio, F b,
                           source_location l) -> task<void> {
                if (r.empty())
                {
                    co_return;
                }

                const auto outer_name = (l.function_name() != nullptr && l.function_name()[0] != '\0')
                                            ? string_view{l.function_name()}
                                            : string_view{"parallel_for"};
                co_await set_task_name{outer_name};

                auto part = Partitioner{};
                auto count = r.size();
                auto workers = tempest::max(1U, sys.worker_count());
                const auto caller_tid = tempest::this_thread::get_id().to_uint64();
                const auto caller_cid = sys.get_profiler().get_or_register_thread().get_current_coroutine_id();

                if constexpr (is_same_v<Partitioner, partitioner::static_chunk>)
                {
                    auto chunk_sz = part.chunk_size;
                    if (chunk_sz == 0)
                    {
                        chunk_sz = tempest::max(size_t{1}, (count + workers - 1) / workers);
                    }
                    auto num_chunks = (count + chunk_sz - 1) / chunk_sz;
                    auto remaining = make_unique<atomic<size_t>>(num_chunks);
                    auto done_event = make_unique<async_event>(sys);

                    auto launch_chunk =
                        [&b,
                         rem = remaining.get(),
                         async_ev = done_event.get()](
                            job_allocator&,
                            job_system&,
                            size_t chunk_start, size_t chunk_end) -> detail::detached_task {
                        if constexpr (requires { b(range<size_t>{chunk_start, chunk_end}); })
                        {
                            b(range<size_t>{chunk_start, chunk_end});
                        }
                        else
                        {
                            for (auto idx = chunk_start; idx < chunk_end; ++idx)
                            {
                                b(idx);
                            }
                        }
                        if (rem->fetch_sub(1, memory_order::acq_rel) == 1)
                        {
                            async_ev->set();
                        }
                        co_return;
                    };

                    for (auto chunk_idx = 0U; chunk_idx < num_chunks; ++chunk_idx)
                    {
                        auto chunk_start = r.first + chunk_idx * chunk_sz;
                        auto chunk_end = tempest::min(r.last, chunk_start + chunk_sz);
                        auto chunk_task = launch_chunk(sys.get_dispatch_allocator(), sys, chunk_start, chunk_end);
                        chunk_task.set_name("parallel_for_chunk");
                        chunk_task.set_source_location(l);
                        chunk_task.set_provenance(caller_tid, caller_cid, caller_tid, caller_cid);
                        sys.schedule(chunk_task.handle, prio, core_class::any);
                    }

                    co_await done_event->wait();
                }
                else
                {
                    auto num_tasks = tempest::min(static_cast<size_t>(workers * 2), count);
                    auto next_idx = make_unique<atomic<size_t>>(r.first);
                    auto remaining = make_unique<atomic<size_t>>(num_tasks);
                    auto done_event = make_unique<async_event>(sys);

                    auto launch_guided =
                        [&b,
                         next = next_idx.get(), r, part, workers, rem = remaining.get(),
                         async_ev = done_event.get()](
                            job_allocator&,
                            job_system&
                            ) -> detail::detached_task {
                        while (true)
                        {
                            auto curr = next->load(memory_order::relaxed);
                            if (curr >= r.last)
                            {
                                break;
                            }
                            auto remaining_items = r.last - curr;
                            auto chunk = tempest::max(part.min_chunk_size, remaining_items / (size_t{2} * workers));
                            chunk = tempest::min(remaining_items, chunk);
                            if (next->compare_exchange_weak(curr, curr + chunk, memory_order::relaxed))
                            {
                                auto chunk_start = curr;
                                auto chunk_end = curr + chunk;
                                if constexpr (requires { b(range<size_t>{chunk_start, chunk_end}); })
                                {
                                    b(range<size_t>{chunk_start, chunk_end});
                                }
                                else
                                {
                                    for (auto idx = chunk_start; idx < chunk_end; ++idx)
                                    {
                                        b(idx);
                                    }
                                }
                            }
                        }
                        if (rem->fetch_sub(1, memory_order::acq_rel) == 1)
                        {
                            async_ev->set();
                        }
                        co_return;
                    };

                    for (auto task_idx = 0U; task_idx < num_tasks; ++task_idx)
                    {
                        auto chunk_task = launch_guided(sys.get_dispatch_allocator(), sys);
                        chunk_task.set_name("parallel_for_chunk");
                        chunk_task.set_source_location(l);
                        chunk_task.set_provenance(caller_tid, caller_cid, caller_tid, caller_cid);
                        sys.schedule(chunk_task.handle, prio, core_class::any);
                    }

                    co_await done_event->wait();
                }
            };

            const auto outer_name = (loc.function_name() != nullptr && loc.function_name()[0] != '\0')
                                        ? string_view{loc.function_name()}
                                        : string_view{"parallel_for"};
            auto task = impl(get_dispatch_allocator(), *this, rng, priority, tempest::move(body), loc);
            task.set_name(outer_name);
            task.set_source_location(loc);
            task.set_profiler(&_profiler);
            return task;
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(range<size_t> rng, F body, source_location loc = source_location::current()) -> task<void>
        {
            return parallel_for<Partitioner>(rng, task_priority::normal, tempest::move(body), loc);
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(size_t count, task_priority priority, F body,
                          source_location loc = source_location::current()) -> task<void>
        {
            return parallel_for<Partitioner>(range<size_t>{0, count}, priority, tempest::move(body), loc);
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(size_t count, F body, source_location loc = source_location::current()) -> task<void>
        {
            return parallel_for<Partitioner>(range<size_t>{0, count}, task_priority::normal, tempest::move(body), loc);
        }

        /// @brief Executes a task graph DAG using borrowed reference semantics.
        /// @details **LIFETIME PRECONDITION**: The caller MUST guarantee that `graph` outlives
        ///          the returned task and remains valid until execution has completed. If the returned
        ///          task is not immediately `co_await`ed or is returned across scopes where `graph` is
        ///          destroyed, worker threads will encounter use-after-free errors.
        ///          This overload is intended for persistent subsystem-owned graphs (e.g. member variables
        ///          reused across frames). For transient or caller-scoped graphs, prefer the by-value
        ///          overload `execute(task_graph)` for compiler-enforced lifetime safety.
        /// @param graph Reference to the task graph to execute.
        /// @return A task resolving to expected<void, error_code>.
        auto execute(task_graph& graph) -> task<expected<void, error_code>>;

        /// @brief Executes a task graph DAG using linear move semantics.
        /// @details Takes full ownership of `graph` by moving it directly into the coroutine frame,
        ///          guaranteeing that the graph and its task nodes remain alive for the exact duration of
        ///          execution with ZERO reliance on caller lifetime discipline.
        ///          Upon completion, the graph is unconditionally returned inside `task_graph_result`
        ///          across both success and error paths, preventing use-after-move hazards and enabling
        ///          immediate inspection, error logging, or multi-frame reuse without reallocation.
        /// @param graph The task graph to execute (transferred by rvalue move).
        /// @return A task resolving to task_graph_result containing the restored graph and execution status.
        auto execute(task_graph&& graph) -> task<task_graph_result, void>;

        auto wait_idle() -> void;
        auto step() -> bool;
        auto step_for(size_t max_tasks) -> size_t;

        template <typename T, typename E = job_error>
        auto when_all(vector<task<T, E>> tasks, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) -> task<vector<expected<T, E>>>;

        template <typename... Tasks>
            requires(sizeof...(Tasks) > 0)
        auto when_all(Tasks&&... tasks) -> task<tuple<typename remove_cvref_t<Tasks>::result_type...>>;

        [[nodiscard]] auto current_worker_core_class() const noexcept -> optional<core_class>;
        [[nodiscard]] auto current_worker_index() const noexcept -> optional<size_t>;

      private:
        logger& _logger;
        profiler::profiler_session& _profiler;
        job_system_config _config{};

        struct impl;
        unique_ptr<impl> _impl;
    };

    template <typename T, typename E>
    inline auto job_context::schedule(task<T, E>& job, task_priority priority, core_class affinity) const -> void
    {
        system->schedule(job, priority, affinity);
    }
} // namespace tempest::job

#endif // tempest_job_job_system_hpp
