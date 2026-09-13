#ifndef tempest_job_work_stealing_deque_hpp
#define tempest_job_work_stealing_deque_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/deque.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/optional.hpp>
#include <tempest/thread.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    template <typename T>
    class concurrent_queue
    {
      public:
        concurrent_queue() = default;
        concurrent_queue(const concurrent_queue&) = delete;
        concurrent_queue(concurrent_queue&&) noexcept = delete;
        ~concurrent_queue() = default;

        auto operator=(const concurrent_queue&) -> concurrent_queue& = delete;
        auto operator=(concurrent_queue&&) noexcept -> concurrent_queue& = delete;

        auto push(T item) -> void
        {
            auto guard = lock_guard{_mutex};
            _items.push_back(tempest::move(item));
        }

        auto pop() -> optional<T>
        {
            auto guard = lock_guard{_mutex};
            if (_items.empty())
            {
                return nullopt;
            }
            auto item = tempest::move(_items.front());
            _items.pop_front();
            return optional<T>{tempest::move(item)};
        }

        template <typename Pred>
        auto pop_if(Pred&& predicate) -> optional<T>
        {
            auto guard = lock_guard{_mutex};
            if (_items.empty())
            {
                return nullopt;
            }
            if (!predicate(_items.front()))
            {
                return nullopt;
            }
            auto item = tempest::move(_items.front());
            _items.pop_front();
            return optional<T>{tempest::move(item)};
        }

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            auto guard = lock_guard{_mutex};
            return _items.empty();
        }

        [[nodiscard]] auto size() const noexcept -> size_t
        {
            auto guard = lock_guard{_mutex};
            return _items.size();
        }

      private:
        mutable mutex _mutex;
        deque<T> _items{};
    };

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

        explicit work_stealing_deque(concurrent_queue<T>& spill_queue) noexcept : _spill_queue{&spill_queue}
        {
        }

        ~work_stealing_deque() = default;

        auto operator=(const work_stealing_deque&) -> work_stealing_deque& = delete;
        auto operator=(work_stealing_deque&&) noexcept -> work_stealing_deque& = delete;

        auto set_spill_queue(concurrent_queue<T>* spill_queue) noexcept -> void
        {
            _spill_queue = spill_queue;
        }

        [[nodiscard]] auto get_spill_queue() const noexcept -> concurrent_queue<T>*
        {
            return _spill_queue;
        }

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

        auto push(T item) -> bool
        {
            if (try_push(item))
            {
                return true;
            }
            if (_spill_queue != nullptr)
            {
                _spill_queue->push(tempest::move(item));
                return true;
            }
            return false;
        }

        auto push_or_spill(T item, concurrent_queue<T>& spill_target) -> void
        {
            if (!try_push(item))
            {
                spill_target.push(tempest::move(item));
            }
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
        concurrent_queue<T>* _spill_queue{nullptr};
    };
} // namespace tempest::job

#endif // tempest_job_work_stealing_deque_hpp
