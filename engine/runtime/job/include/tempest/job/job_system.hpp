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
                      core_class affinity = core_class::any) -> void
        {
            if (job.get_profiler() == nullptr)
            {
                job.set_profiler(&_profiler);
            }

            schedule(job.handle(), priority, affinity);
        }

        template <typename T, typename E>
        auto schedule(const job_context& ctx, task<T, E>& job, task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) -> void
        {
            if (job.get_profiler() == nullptr)
            {
                job.set_profiler(&_profiler);
            }
            schedule(ctx, job.handle(), priority, affinity);
        }

        template <typename F>
        auto async(F&& callable)
        {
            return async(task_priority::normal, core_class::any, forward<F>(callable));
        }

        template <typename F>
        auto async(task_priority priority, core_class affinity, F&& callable)
        {
            using ReturnType = invoke_result_t<F>;

            if constexpr (detail::is_task_v<ReturnType>)
            {
                // These references are safe, as they are stable across the lifetime of the coroutine, and the coroutine
                // is guaranteed to be alive until it is completed. These references aren't used in the body of the
                // lambda.
                //
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
                job.set_profiler(&_profiler);
                schedule(job.handle(), priority, affinity);
                return job;
            }
            else if constexpr (is_void_v<ReturnType>)
            {
                // These references are safe, as they are stable across the lifetime of the coroutine, and the coroutine
                // is guaranteed to be alive until it is completed. These references aren't used in the body of the
                // lambda.
                //
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
                auto runner = [](job_allocator&, job_system&, F func) -> task<void> {
                    func();
                    co_return;
                };

                auto job = runner(get_dispatch_allocator(), *this, forward<F>(callable));
                job.set_profiler(&_profiler);
                schedule(job.handle(), priority, affinity);
                return job;
            }
            else
            {
                // These references are safe, as they are stable across the lifetime of the coroutine, and the coroutine
                // is guaranteed to be alive until it is completed. These references aren't used in the body of the
                // lambda.
                //
                // NOLINTNEXTLINE(cppcoreguidelines-avoid-reference-coroutine-parameters)
                auto runner = [](job_allocator&, job_system&, F func) -> task<ReturnType> { co_return func(); };

                auto job = runner(get_dispatch_allocator(), *this, forward<F>(callable));
                job.set_profiler(&_profiler);
                schedule(job.handle(), priority, affinity);
                return job;
            }
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(range<size_t> rng, task_priority priority, F body) -> task<void>
        {
            if (rng.empty())
            {
                co_return;
            }

            auto part = Partitioner{};
            auto count = rng.size();
            auto workers = tempest::max(1U, worker_count());

            if constexpr (is_same_v<Partitioner, partitioner::static_chunk>)
            {
                auto chunk_sz = part.chunk_size;
                if (chunk_sz == 0)
                {
                    chunk_sz = tempest::max(size_t{1}, (count + workers - 1) / workers);
                }
                auto num_chunks = (count + chunk_sz - 1) / chunk_sz;
                auto remaining = make_unique<atomic<size_t>>(num_chunks);
                auto done_event = make_unique<async_event>(*this);

                auto launch_chunk =
                    [&body, // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
                     rem = remaining.get(),
                     async_ev = done_event.get()](
                        job_allocator&, // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                        job_system&,    // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                        size_t chunk_start, size_t chunk_end) -> detail::detached_task {
                    if constexpr (requires { body(range<size_t>{chunk_start, chunk_end}); })
                    {
                        body(range<size_t>{chunk_start, chunk_end});
                    }
                    else
                    {
                        for (auto idx = chunk_start; idx < chunk_end; ++idx)
                        {
                            body(idx);
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
                    auto chunk_start = rng.first + chunk_idx * chunk_sz;
                    auto chunk_end = tempest::min(rng.last, chunk_start + chunk_sz);
                    auto task = launch_chunk(get_dispatch_allocator(), *this, chunk_start, chunk_end);
                    schedule(task.handle, priority, core_class::any);
                }

                co_await done_event->wait();
            }
            else
            {
                auto num_tasks = tempest::min(static_cast<size_t>(workers * 2), count);
                auto next_idx = make_unique<atomic<size_t>>(rng.first);
                auto remaining = make_unique<atomic<size_t>>(num_tasks);
                auto done_event = make_unique<async_event>(*this);

                auto launch_guided =
                    [&body, // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
                     next = next_idx.get(), rng, part, workers, rem = remaining.get(),
                     async_ev = done_event.get()](
                        job_allocator&, // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                        job_system&     // NOLINT(cppcoreguidelines-avoid-reference-coroutine-parameters)
                        ) -> detail::detached_task {
                    while (true)
                    {
                        auto curr = next->load(memory_order::relaxed);
                        if (curr >= rng.last)
                        {
                            break;
                        }
                        auto remaining_items = rng.last - curr;
                        auto chunk = tempest::max(part.min_chunk_size, remaining_items / (2ULL * workers));
                        chunk = tempest::min(remaining_items, chunk);
                        if (next->compare_exchange_weak(curr, curr + chunk, memory_order::relaxed))
                        {
                            auto chunk_start = curr;
                            auto chunk_end = curr + chunk;
                            if constexpr (requires { body(range<size_t>{chunk_start, chunk_end}); })
                            {
                                body(range<size_t>{chunk_start, chunk_end});
                            }
                            else
                            {
                                for (auto idx = chunk_start; idx < chunk_end; ++idx)
                                {
                                    body(idx);
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
                    auto task = launch_guided(get_dispatch_allocator(), *this);
                    schedule(task.handle, priority, core_class::any);
                }

                co_await done_event->wait();
            }
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(range<size_t> rng, F body) -> task<void>
        {
            return parallel_for<Partitioner>(rng, task_priority::normal, tempest::move(body));
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(size_t count, task_priority priority, F body) -> task<void>
        {
            return parallel_for<Partitioner>(range<size_t>{0, count}, priority, tempest::move(body));
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(size_t count, F body) -> task<void>
        {
            return parallel_for<Partitioner>(range<size_t>{0, count}, task_priority::normal, tempest::move(body));
        }

        auto execute(task_graph& graph) -> task<expected<void, error_code>>;

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
