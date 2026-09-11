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
        auto when_all_leaf_runner(TaskType t, ResultType* out, atomic<size_t>* remaining, async_event* done_event)
            -> task<void>
        {
            struct raw_task_awaiter
            {
                TaskType& child;
                auto await_ready() const noexcept -> bool
                {
                    return child.is_ready();
                }
                auto await_suspend(coroutine_handle<> h) noexcept -> coroutine_handle<>
                {
                    child.handle().promise().continuation = h;
                    return child.handle();
                }
                auto await_resume() noexcept -> void
                {
                }
            };

            co_await raw_task_awaiter{t};
            *out = tempest::move(t.result());
            if (remaining->fetch_sub(1, memory_order::acq_rel) == 1)
            {
                done_event->set();
            }
        }
    } // namespace detail

    template <typename T, typename E = job_error>
    auto when_all(vector<task<T, E>> tasks) -> task<vector<expected<T, E>>>
    {
        auto count = tasks.size();
        if (count == 0)
        {
            co_return vector<expected<T, E>>{};
        }

        auto results = vector<expected<T, E>>{};
        results.resize(count);

        auto remaining = make_unique<atomic<size_t>>(count);
        auto done_event = make_unique<async_event>();

        auto runners = vector<task<void>>{};
        runners.reserve(count);

        auto* js = job_system::get_current();

        for (auto i = 0u; i < count; ++i)
        {
            runners.push_back(
                detail::when_all_leaf_runner(tempest::move(tasks[i]), &results[i], remaining.get(), done_event.get()));
            if (js != nullptr)
            {
                js->schedule(runners.back().handle());
            }
            else
            {
                runners.back().resume();
            }
        }

        co_await done_event->wait();
        co_return tempest::move(results);
    }

    template <typename... Tasks>
        requires(sizeof...(Tasks) > 0)
    auto when_all(Tasks&&... tasks) -> task<tuple<typename remove_cvref_t<Tasks>::result_type...>>
    {
        constexpr auto count = sizeof...(Tasks);

        using ResultTuple = tuple<typename remove_cvref_t<Tasks>::result_type...>;
        auto results = make_unique<ResultTuple>();

        auto remaining = make_unique<atomic<size_t>>(count);
        auto done_event = make_unique<async_event>();

        auto runners = vector<task<void>>{};
        runners.reserve(count);

        auto* js = job_system::get_current();

        auto launch_leaf = [&runners, js, rem = remaining.get(), ev = done_event.get()]<typename Tsk, typename Res>(
                               Tsk&& tsk, Res* out) {
            runners.push_back(
                detail::when_all_leaf_runner(tempest::forward<Tsk>(tsk), out, rem, ev));
            if (js != nullptr)
            {
                js->schedule(runners.back().handle());
            }
            else
            {
                runners.back().resume();
            }
        };

        [&]<size_t... Is>(index_sequence<Is...>) {
            (launch_leaf(tempest::forward<Tasks>(tasks), &get<Is>(*results)), ...);
        }(make_index_sequence<count>{});

        co_await done_event->wait();
        co_return tempest::move(*results);
    }
} // namespace tempest::job

#endif // tempest_job_when_all_hpp
