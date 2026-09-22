#ifndef tempest_job_work_stealing_deque_hpp
#define tempest_job_work_stealing_deque_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    inline constexpr size_t default_work_stealing_deque_capacity = 1024;

    template <typename T, size_t Capacity = default_work_stealing_deque_capacity>
    class work_stealing_deque
    {
        static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                      "Capacity must be a power of two and >= 2");
        static constexpr size_t Mask = Capacity - 1;

      public:
        using value_type = T;

        work_stealing_deque() = default;
        work_stealing_deque(const work_stealing_deque&) = delete;
        work_stealing_deque(work_stealing_deque&&) noexcept = delete;
        ~work_stealing_deque() = default;

        auto operator=(const work_stealing_deque&) -> work_stealing_deque& = delete;
        auto operator=(work_stealing_deque&&) noexcept -> work_stealing_deque& = delete;

        [[nodiscard]] constexpr auto capacity() const noexcept -> size_t
        {
            return Capacity;
        }

        [[nodiscard]] auto size() const noexcept -> size_t
        {
            const auto bottom_index = _bottom.load(memory_order::relaxed);
            const auto top_index = _top.load(memory_order::relaxed);
            return (bottom_index >= top_index) ? static_cast<size_t>(bottom_index - top_index) : 0;
        }

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            const auto bottom_index = _bottom.load(memory_order::relaxed);
            const auto top_index = _top.load(memory_order::relaxed);
            return bottom_index <= top_index;
        }

        auto try_push(T item) -> bool
        {
            const auto bottom_index = _bottom.load(memory_order::relaxed);
            const auto top_index = _top.load(memory_order::acquire);
            if (bottom_index - top_index >= static_cast<int64_t>(Capacity))
            {
                return false;
            }
            _buffer[static_cast<size_t>(bottom_index & Mask)].store(item, memory_order::relaxed);
            _bottom.store(bottom_index + 1, memory_order::release);
            return true;
        }

        auto pop() -> optional<T>
        {
            auto bottom_index = _bottom.load(memory_order::relaxed) - 1;
            _bottom.store(bottom_index, memory_order::seq_cst);
            const auto top_index = _top.load(memory_order::seq_cst);
            if (top_index <= bottom_index)
            {
                auto item = _buffer[static_cast<size_t>(bottom_index & Mask)].load(memory_order::relaxed);
                if (top_index != bottom_index)
                {
                    return optional<T>{item};
                }

                auto expected_top_index = top_index;
                if (_top.compare_exchange_strong(expected_top_index, top_index + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    _bottom.store(bottom_index + 1, memory_order::relaxed);
                    return optional<T>{item};
                }
                _bottom.store(bottom_index + 1, memory_order::relaxed);
                return nullopt;
            }
            _bottom.store(bottom_index + 1, memory_order::relaxed);
            return nullopt;
        }

        auto steal() -> optional<T>
        {
            const auto top_index = _top.load(memory_order::acquire);
            atomic_thread_fence(memory_order::seq_cst);
            const auto bottom_index = _bottom.load(memory_order::acquire);
            if (top_index < bottom_index)
            {
                auto item = _buffer[static_cast<size_t>(top_index & Mask)].load(memory_order::relaxed);
                auto expected_top = top_index;
                if (_top.compare_exchange_strong(expected_top, top_index + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    return optional<T>{item};
                }
                return nullopt;
            }
            return nullopt;
        }

        template <typename Pred>
        auto steal_if(Pred&& predicate) -> optional<T>
        {
            const auto top_index = _top.load(memory_order::acquire);
            atomic_thread_fence(memory_order::seq_cst);
            const auto bottom_index = _bottom.load(memory_order::acquire);
            if (top_index < bottom_index)
            {
                auto item = _buffer[static_cast<size_t>(top_index & Mask)].load(memory_order::relaxed);
                if (!predicate(item))
                {
                    return nullopt;
                }
                auto expected_top_index = top_index;
                if (_top.compare_exchange_strong(expected_top_index, top_index + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    return optional<T>{item};
                }
                return nullopt;
            }
            return nullopt;
        }

      private:
        alignas(hardware_destructive_interference_size) atomic<int64_t> _top{0};
        alignas(hardware_destructive_interference_size) atomic<int64_t> _bottom{0};
        array<atomic<T>, Capacity> _buffer{};
    };
} // namespace tempest::job

#endif // tempest_job_work_stealing_deque_hpp
