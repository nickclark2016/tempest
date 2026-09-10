#ifndef tempest_job_job_system_hpp
#define tempest_job_job_system_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/coroutine.hpp>
#include <tempest/deque.hpp>
#include <tempest/int.hpp>
#include <tempest/job/allocator.hpp>
#include <tempest/job/task.hpp>
#include <tempest/job/types.hpp>
#include <tempest/logger.hpp>
#include <tempest/memory.hpp>
#include <tempest/mutex.hpp>
#include <tempest/optional.hpp>
#include <tempest/profiler/session.hpp>
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
    };

    namespace detail
    {
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
    } // namespace detail

    class TEMPEST_API job_system
    {
      public:
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

        auto schedule(coroutine_handle<> handle, task_priority priority = task_priority::normal,
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
                auto runner = [](F fn) -> ReturnType {
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

                auto t = runner(forward<F>(callable));
                schedule(t.handle(), priority, affinity);
                return t;
            }
            else if constexpr (is_void_v<ReturnType>)
            {
                auto runner = [](F fn) -> task<void> {
                    fn();
                    co_return;
                };

                auto t = runner(forward<F>(callable));
                schedule(t.handle(), priority, affinity);
                return t;
            }
            else
            {
                auto runner = [](F fn) -> task<ReturnType> {
                    co_return fn();
                };

                auto t = runner(forward<F>(callable));
                schedule(t.handle(), priority, affinity);
                return t;
            }
        }

        auto wait_idle() -> void;
        auto step() -> bool;
        auto step_for(size_t max_tasks) -> size_t;

        [[nodiscard]] static auto get_current() noexcept -> job_system*;

      private:
        struct queue_item
        {
            coroutine_handle<> handle{nullptr};
            task_priority priority{task_priority::normal};
            core_class affinity{core_class::any};
        };

        logger& _logger;
        profiler::profiler_session& _profiler;
        job_system_config _config{};

        mutable mutex _queue_mutex{};
        array<deque<queue_item>, static_cast<size_t>(task_priority::count)> _queues{};
        uint32_t _anti_starvation_counter{0};
        job_allocator _allocator{};
    };
} // namespace tempest::job

#endif // tempest_job_job_system_hpp
