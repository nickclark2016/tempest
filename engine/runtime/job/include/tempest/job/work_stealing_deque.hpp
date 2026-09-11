#ifndef tempest_job_work_stealing_deque_hpp
#define tempest_job_work_stealing_deque_hpp

#include <tempest/api.hpp>
#include <tempest/array.hpp>
#include <tempest/atomic.hpp>
#include <tempest/deque.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/optional.hpp>
#include <tempest/utility.hpp>

namespace tempest::job
{
    template <typename T>
    class concurrent_queue
    {
      public:
        concurrent_queue() = default;
        ~concurrent_queue() = default;

        concurrent_queue(const concurrent_queue&) = delete;
        concurrent_queue& operator=(const concurrent_queue&) = delete;

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
        mutable mutex _mutex{};
        deque<T> _items{};
    };

    template <typename T, size_t Capacity = 1024>
    class work_stealing_deque
    {
        static_assert((Capacity & (Capacity - 1)) == 0 && Capacity >= 2,
                      "Capacity must be a power of two and >= 2");
        static constexpr size_t Mask = Capacity - 1;

      public:
        using value_type = T;

        work_stealing_deque() = default;

        explicit work_stealing_deque(concurrent_queue<T>& spill_queue) noexcept : _spill_queue{&spill_queue}
        {
        }

        ~work_stealing_deque() = default;

        work_stealing_deque(const work_stealing_deque&) = delete;
        work_stealing_deque& operator=(const work_stealing_deque&) = delete;

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
            auto b = _bottom.load(memory_order::relaxed);
            auto t = _top.load(memory_order::relaxed);
            return (b >= t) ? static_cast<size_t>(b - t) : 0;
        }

        [[nodiscard]] auto empty() const noexcept -> bool
        {
            auto b = _bottom.load(memory_order::relaxed);
            auto t = _top.load(memory_order::relaxed);
            return b <= t;
        }

        auto try_push(T item) -> bool
        {
            auto b = _bottom.load(memory_order::relaxed);
            auto t = _top.load(memory_order::acquire);
            if (b - t >= static_cast<int64_t>(Capacity))
            {
                return false;
            }
            _buffer[static_cast<size_t>(b & Mask)] = tempest::move(item);
            atomic_thread_fence(memory_order::release);
            _bottom.store(b + 1, memory_order::relaxed);
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
            auto b = _bottom.load(memory_order::relaxed) - 1;
            _bottom.store(b, memory_order::seq_cst);
            auto t = _top.load(memory_order::seq_cst);
            if (t <= b)
            {
                auto item = tempest::move(_buffer[static_cast<size_t>(b & Mask)]);
                if (t != b)
                {
                    return optional<T>{tempest::move(item)};
                }

                auto expected_t = t;
                if (_top.compare_exchange_strong(expected_t, t + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    _bottom.store(b + 1, memory_order::relaxed);
                    return optional<T>{tempest::move(item)};
                }
                _bottom.store(b + 1, memory_order::relaxed);
                return nullopt;
            }
            _bottom.store(b + 1, memory_order::relaxed);
            return nullopt;
        }

        auto steal() -> optional<T>
        {
            auto t = _top.load(memory_order::acquire);
            atomic_thread_fence(memory_order::seq_cst);
            auto b = _bottom.load(memory_order::acquire);
            if (t < b)
            {
                auto item = _buffer[static_cast<size_t>(t & Mask)];
                auto expected_t = t;
                if (_top.compare_exchange_strong(expected_t, t + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    return optional<T>{tempest::move(item)};
                }
                return nullopt;
            }
            return nullopt;
        }

        template <typename Pred>
        auto steal_if(Pred&& predicate) -> optional<T>
        {
            auto t = _top.load(memory_order::acquire);
            atomic_thread_fence(memory_order::seq_cst);
            auto b = _bottom.load(memory_order::acquire);
            if (t < b)
            {
                const auto& item = _buffer[static_cast<size_t>(t & Mask)];
                if (!predicate(item))
                {
                    return nullopt;
                }
                auto item_copy = item;
                auto expected_t = t;
                if (_top.compare_exchange_strong(expected_t, t + 1, memory_order::seq_cst, memory_order::relaxed))
                {
                    return optional<T>{tempest::move(item_copy)};
                }
                return nullopt;
            }
            return nullopt;
        }

      private:
        alignas(64) atomic<int64_t> _top{0};
        alignas(64) atomic<int64_t> _bottom{0};
        array<T, Capacity> _buffer{};
        concurrent_queue<T>* _spill_queue{nullptr};
    };
} // namespace tempest::job

#endif // tempest_job_work_stealing_deque_hpp
