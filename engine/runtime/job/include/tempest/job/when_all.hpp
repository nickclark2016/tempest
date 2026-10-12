#ifndef tempest_job_when_all_hpp
#define tempest_job_when_all_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/expected.hpp>
#include <tempest/int.hpp>
#include <tempest/job/async_event.hpp>
#include <tempest/job/job_system.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/memory.hpp>
#include <tempest/tuple.hpp>
#include <tempest/type_traits.hpp>
#include <tempest/utility.hpp>
#include <tempest/vector.hpp>

namespace tempest::job
{
    namespace detail
    {
        template <typename TaskType, typename ResultType>
        auto when_all_leaf_runner(job_allocator* /*alloc*/, TaskType type, ResultType* out, atomic<size_t>* remaining,
                                  async_event* done_event, uint64_t parent_coroutine_id = 0) -> detached_task
        {
            const auto child_name =
                (!type.name().empty() && type.name() != "task") ? type.name() : string_view{"when_all_leaf"};
            co_await set_task_name{child_name};

            struct raw_task_awaiter
            {
                non_null<TaskType> child;
                uint64_t parent_cid{0};

                [[nodiscard]] auto await_ready() const noexcept -> bool
                {
                    return child->is_ready();
                }

                auto await_suspend(coroutine_handle<detached_task::promise_type> hnd) noexcept -> coroutine_handle<>
                {
                    child->handle().promise().continuation = hnd;
                    if (child->handle().promise().session == nullptr)
                    {
                        child->handle().promise().session = hnd.promise().session;
                    }
                    if (parent_cid != 0)
                    {
                        hnd.promise().spawned_by_coroutine_id = parent_cid;
                        hnd.promise().awaited_by_coroutine_id = parent_cid;
                    }
                    child->handle().promise().awaited_by_thread_id = tempest::this_thread::get_id().to_uint64();
                    child->handle().promise().awaited_by_coroutine_id =
                        parent_cid != 0 ? parent_cid : hnd.promise().coroutine_id;
                    if (child->handle().promise().spawned_by_coroutine_id == 0 && parent_cid != 0)
                    {
                        child->handle().promise().spawned_by_coroutine_id = parent_cid;
                    }
                    return child->handle();
                }

                auto await_resume() noexcept -> void
                {
                }
            };

            co_await raw_task_awaiter{type, parent_coroutine_id};
            *out = tempest::move(type.result());
            if (remaining->fetch_sub(1, memory_order::acq_rel) == 1)
            {
                done_event->set();
            }
            co_return;
        }

        template <typename TaskType, typename ResultType>
        auto when_all_leaf_runner(TaskType type, ResultType* out, atomic<size_t>* remaining, async_event* done_event,
                                  uint64_t parent_coroutine_id = 0)
            -> detached_task
        {
            return when_all_leaf_runner(nullptr, tempest::move(type), out, remaining, done_event, parent_coroutine_id);
        }

        template <typename T, typename E>
        auto await_when_all_vector(job_allocator* /*alloc*/, vector<expected<T, E>> results,
                                   unique_ptr<atomic<size_t>> /*remaining*/,
                                   unique_ptr<async_event> done_event) -> task<vector<expected<T, E>>>
        {
            if (done_event != nullptr)
            {
                co_await done_event->wait();
            }
            co_return tempest::move(results);
        }

        template <typename ResultTuple>
        auto await_when_all_tuple(job_allocator* /*alloc*/, unique_ptr<ResultTuple> results,
                                  unique_ptr<atomic<size_t>> /*remaining*/,
                                  unique_ptr<async_event> done_event) -> task<ResultTuple>
        {
            if (done_event != nullptr)
            {
                co_await done_event->wait();
            }
            co_return tempest::move(*results);
        }
    } // namespace detail

    template <typename T, typename E = job_error>
    auto when_all(job_system& sys, vector<task<T, E>> tasks, task_priority priority = task_priority::normal,
                  core_class affinity = core_class::any) -> task<vector<expected<T, E>>>
    {
        const auto count = tasks.size();
        if (count == 0)
        {
            return detail::await_when_all_vector<T, E>(&sys.get_dispatch_allocator(), vector<expected<T, E>>{},
                                                       nullptr, nullptr);
        }

        auto results = vector<expected<T, E>>{};
        results.resize(count);

        auto remaining = make_unique<atomic<size_t>>(count);
        auto done_event = make_unique<async_event>(sys);

        const auto caller_tid = tempest::this_thread::get_id().to_uint64();
        const auto parent_cid = sys.get_profiler().is_enabled()
                                    ? sys.get_profiler().get_or_register_thread().get_current_coroutine_id()
                                    : 0ULL;

        for (auto i = 0U; i < count; ++i)
        {
            if (tasks[i].get_profiler() == nullptr)
            {
                tasks[i].set_profiler(&sys.get_profiler());
            }
            const auto child_name = (!tasks[i].name().empty() && tasks[i].name() != "task")
                                        ? tasks[i].name()
                                        : string_view{"when_all_leaf"};
            auto runner = detail::when_all_leaf_runner(&sys.get_dispatch_allocator(), tempest::move(tasks[i]),
                                                       &results[i], remaining.get(), done_event.get(), parent_cid);
            runner.set_name(child_name);
            runner.set_provenance(caller_tid, parent_cid, caller_tid, parent_cid);
            sys.schedule(runner.handle, priority, affinity);
        }

        return detail::await_when_all_vector<T, E>(&sys.get_dispatch_allocator(), tempest::move(results),
                                                   tempest::move(remaining), tempest::move(done_event));
    }

    template <typename T, typename E = job_error>
    auto when_all(vector<task<T, E>> tasks) -> task<vector<expected<T, E>>>
    {
        const auto count = tasks.size();
        if (count == 0)
        {
            return detail::await_when_all_vector<T, E>(nullptr, vector<expected<T, E>>{}, nullptr, nullptr);
        }

        auto results = vector<expected<T, E>>{};
        results.resize(count);

        auto remaining = make_unique<atomic<size_t>>(count);
        auto done_event = make_unique<async_event>();

        const auto caller_tid = tempest::this_thread::get_id().to_uint64();

        for (auto i = 0U; i < count; ++i)
        {
            const auto child_name = (!tasks[i].name().empty() && tasks[i].name() != "task")
                                        ? tasks[i].name()
                                        : string_view{"when_all_leaf"};
            auto runner =
                detail::when_all_leaf_runner(tempest::move(tasks[i]), &results[i], remaining.get(), done_event.get(), 0);
            runner.set_name(child_name);
            runner.set_provenance(caller_tid, 0, caller_tid, 0);
            runner.handle.resume();
        }

        return detail::await_when_all_vector<T, E>(nullptr, tempest::move(results), tempest::move(remaining),
                                                   tempest::move(done_event));
    }

    template <typename... Tasks>
        requires(sizeof...(Tasks) > 0)
    auto when_all(job_system& sys, Tasks&&... tasks) -> task<tuple<typename remove_cvref_t<Tasks>::result_type...>>
    {
        constexpr auto count = sizeof...(Tasks);

        using ResultTuple = tuple<typename remove_cvref_t<Tasks>::result_type...>;
        auto results = make_unique<ResultTuple>();

        auto remaining = make_unique<atomic<size_t>>(count);
        auto done_event = make_unique<async_event>(sys);

        const auto caller_tid = tempest::this_thread::get_id().to_uint64();
        const auto parent_cid = sys.get_profiler().is_enabled()
                                    ? sys.get_profiler().get_or_register_thread().get_current_coroutine_id()
                                    : 0ULL;

        auto launch_leaf = [&sys, rem = remaining.get(), async_ev = done_event.get(), caller_tid,
                            parent_cid]<typename Tsk, typename Res>(Tsk&& tsk, Res* out) -> auto {
            if (tsk.get_profiler() == nullptr)
            {
                tsk.set_profiler(&sys.get_profiler());
            }
            const auto child_name = (!tsk.name().empty() && tsk.name() != "task")
                                        ? tsk.name()
                                        : string_view{"when_all_leaf"};
            auto runner = detail::when_all_leaf_runner(&sys.get_dispatch_allocator(), tempest::forward<Tsk>(tsk), out,
                                                       rem, async_ev, parent_cid);
            runner.set_name(child_name);
            runner.set_provenance(caller_tid, parent_cid, caller_tid, parent_cid);
            sys.schedule(runner.handle);
        };

        [&]<size_t... Is>(index_sequence<Is...>) {
            (launch_leaf(tempest::forward<Tasks>(tasks), &get<Is>(*results)), ...);
        }(make_index_sequence<count>{});

        return detail::await_when_all_tuple(&sys.get_dispatch_allocator(), tempest::move(results),
                                            tempest::move(remaining), tempest::move(done_event));
    }

    template <typename FirstTask, typename... Tasks>
        requires(!is_same_v<remove_cvref_t<FirstTask>, job_system>)
    auto when_all(FirstTask&& first, Tasks&&... rest)
        -> task<tuple<typename remove_cvref_t<FirstTask>::result_type, typename remove_cvref_t<Tasks>::result_type...>>
    {
        constexpr auto count = 1 + sizeof...(Tasks);

        using ResultTuple =
            tuple<typename remove_cvref_t<FirstTask>::result_type, typename remove_cvref_t<Tasks>::result_type...>;
        auto results = make_unique<ResultTuple>();

        auto remaining = make_unique<atomic<size_t>>(count);
        auto done_event = make_unique<async_event>();

        const auto caller_tid = tempest::this_thread::get_id().to_uint64();

        auto launch_leaf = [rem = remaining.get(), async_ev = done_event.get(),
                            caller_tid]<typename Tsk, typename Res>(Tsk&& tsk, Res* out) -> auto {
            const auto child_name = (!tsk.name().empty() && tsk.name() != "task")
                                        ? tsk.name()
                                        : string_view{"when_all_leaf"};
            auto runner = detail::when_all_leaf_runner(tempest::forward<Tsk>(tsk), out, rem, async_ev, 0);
            runner.set_name(child_name);
            runner.set_provenance(caller_tid, 0, caller_tid, 0);
            runner.handle.resume();
        };

        launch_leaf(tempest::forward<FirstTask>(first), &get<0>(*results));
        [&]<size_t... Is>(index_sequence<Is...>) {
            (launch_leaf(tempest::forward<Tasks>(rest), &get<Is + 1>(*results)), ...);
        }(make_index_sequence<sizeof...(Tasks)>{});

        return detail::await_when_all_tuple(nullptr, tempest::move(results), tempest::move(remaining),
                                            tempest::move(done_event));
    }

    template <typename T, typename E>
    inline auto job_system::when_all(vector<task<T, E>> tasks, task_priority priority, core_class affinity)
        -> task<vector<expected<T, E>>>
    {
        return tempest::job::when_all(*this, tempest::move(tasks), priority, affinity);
    }

    template <typename... Tasks>
        requires(sizeof...(Tasks) > 0)
    inline auto job_system::when_all(Tasks&&... tasks) -> task<tuple<typename remove_cvref_t<Tasks>::result_type...>>
    {
        return tempest::job::when_all(*this, tempest::forward<Tasks>(tasks)...);
    }
} // namespace tempest::job

#endif // tempest_job_when_all_hpp
