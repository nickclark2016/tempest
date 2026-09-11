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
        optional<uint32_t> performance_worker_count{nullopt};
        optional<uint32_t> efficiency_worker_count{nullopt};
        bool enable_work_stealing{true};
        bool enable_core_pinning{true};
        uint32_t starvation_quantum{16};
        optional<cpu_topology> topology{nullopt};
    };

    class TEMPEST_API job_system
    {
      public:
        struct queue_item
        {
            coroutine_handle<> handle{nullptr};
            task_priority priority{task_priority::normal};
            core_class affinity{core_class::any};
        };

        job_system(logger& log, profiler::profiler_session& profiler, const job_system_config& config = {});
        ~job_system();

        job_system(const job_system&) = delete;
        job_system& operator=(const job_system&) = delete;
        job_system(job_system&&) = delete;
        job_system& operator=(job_system&&) = delete;

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
        auto schedule(const job_context& ctx, coroutine_handle<> handle,
                      task_priority priority = task_priority::normal,
                      core_class affinity = core_class::any) -> void;

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
                auto runner = [](job_allocator&, F fn) -> ReturnType {
                    if constexpr (is_void_v<typename ReturnType::value_type>)
                    {
                        co_await fn();
                        co_return;
                    }
                    else
                    {
                        co_return co_await fn();
                    }
                };

                auto t = runner(get_dispatch_allocator(), forward<F>(callable));
                schedule(t.handle(), priority, affinity);
                return t;
            }
            else if constexpr (is_void_v<ReturnType>)
            {
                auto runner = [](job_allocator&, F fn) -> task<void> {
                    fn();
                    co_return;
                };

                auto t = runner(get_dispatch_allocator(), forward<F>(callable));
                schedule(t.handle(), priority, affinity);
                return t;
            }
            else
            {
                auto runner = [](job_allocator&, F fn) -> task<ReturnType> {
                    co_return fn();
                };

                auto t = runner(get_dispatch_allocator(), forward<F>(callable));
                schedule(t.handle(), priority, affinity);
                return t;
            }
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(range<size_t> r, task_priority priority, F body) -> task<void>
        {
            if (r.empty())
            {
                co_return;
            }

            auto part = Partitioner{};
            auto count = r.size();
            auto workers = tempest::max(1u, worker_count());

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

                auto launch_chunk = [&body, rem = remaining.get(), ev = done_event.get()](
                                        job_allocator&, size_t chunk_start, size_t chunk_end) -> detail::detached_task {
                    if constexpr (requires { body(range<size_t>{chunk_start, chunk_end}); })
                    {
                        body(range<size_t>{chunk_start, chunk_end});
                    }
                    else
                    {
                        for (auto i = chunk_start; i < chunk_end; ++i)
                        {
                            body(i);
                        }
                    }
                    if (rem->fetch_sub(1, memory_order::acq_rel) == 1)
                    {
                        ev->set();
                    }
                    co_return;
                };

                for (auto c = 0u; c < num_chunks; ++c)
                {
                    auto chunk_start = r.first + c * chunk_sz;
                    auto chunk_end = tempest::min(r.last, chunk_start + chunk_sz);
                    auto task = launch_chunk(get_dispatch_allocator(), chunk_start, chunk_end);
                    schedule(task.handle, priority, core_class::any);
                }

                co_await done_event->wait();
            }
            else
            {
                auto num_tasks = tempest::min(static_cast<size_t>(workers * 2), count);
                auto next_idx = make_unique<atomic<size_t>>(r.first);
                auto remaining = make_unique<atomic<size_t>>(num_tasks);
                auto done_event = make_unique<async_event>(*this);

                auto launch_guided = [&body, next = next_idx.get(), r, part, workers,
                                      rem = remaining.get(), ev = done_event.get()](job_allocator&) -> detail::detached_task {
                    while (true)
                    {
                        auto curr = next->load(memory_order::relaxed);
                        if (curr >= r.last)
                        {
                            break;
                        }
                        auto remaining_items = r.last - curr;
                        auto chunk = tempest::max(part.min_chunk_size, remaining_items / (2 * workers));
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
                                for (auto i = chunk_start; i < chunk_end; ++i)
                                {
                                    body(i);
                                }
                            }
                        }
                    }
                    if (rem->fetch_sub(1, memory_order::acq_rel) == 1)
                    {
                        ev->set();
                    }
                    co_return;
                };

                for (auto t = 0u; t < num_tasks; ++t)
                {
                    auto task = launch_guided(get_dispatch_allocator());
                    schedule(task.handle, priority, core_class::any);
                }

                co_await done_event->wait();
            }
        }

        template <typename Partitioner = partitioner::guided, typename F>
        auto parallel_for(range<size_t> r, F body) -> task<void>
        {
            return parallel_for<Partitioner>(r, task_priority::normal, tempest::move(body));
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
        auto when_all(vector<task<T, E>> tasks) -> task<vector<expected<T, E>>>;

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
} // namespace tempest::job

#endif // tempest_job_job_system_hpp
