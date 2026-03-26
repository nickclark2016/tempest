#ifndef tempest_event_event_queue_hpp
#define tempest_event_event_queue_hpp

/// @file event_queue.hpp
/// @brief Thread-safe, typed event queue for deferred event processing.
///
/// An event_queue<T> buffers events of type T for later processing. Events are enqueued from
/// any thread (protected by mutex) and drained on a caller-chosen thread. The drain operation
/// atomically swaps the internal buffer with a local copy, then processes outside the lock.
/// This means events published during a drain are queued for the next drain cycle.
///
/// Ownership: Subsystems own their event_queue instances as members or stack objects.
/// The queue must be unsubscribed from its event_dispatcher before destruction.
///
/// Non-copyable, non-moveable: address stability is required because the dispatcher holds
/// a non-owning pointer to this queue.

#include <tempest/event.hpp>
#include <tempest/mutex.hpp>
#include <tempest/vector.hpp>

namespace tempest::event
{
    /// @brief Thread-safe queue that buffers events of type T for deferred processing.
    /// @tparam T Event type. Must satisfy the event_type concept.
    template <event_type T>
    class event_queue
    {
      public:
        event_queue() noexcept = default;

        event_queue(const event_queue&) = delete;
        event_queue(event_queue&&) = delete;
        event_queue& operator=(const event_queue&) = delete;
        event_queue& operator=(event_queue&&) = delete;

        ~event_queue() = default;

        /// @brief Enqueue an event from any thread.
        /// Acquires the internal mutex, appends the event, and releases.
        /// @param evt The event to enqueue.
        auto enqueue(const T& evt) noexcept -> void
        {
            lock_guard lock(_mutex);
            _pending.push_back(evt);
        }

        /// @brief Drain all pending events, invoking a callable for each.
        /// Atomically swaps the internal buffer under lock, then processes outside the lock.
        /// Events enqueued during drain land in the (now-empty) internal buffer for the next cycle.
        /// @tparam Fn Callable type accepting const T&.
        /// @param handler The function to invoke for each drained event.
        template <typename Fn>
            requires is_invocable_v<Fn, const T&>
        auto drain(Fn&& handler) noexcept(is_nothrow_invocable_v<Fn, const T&>) -> void
        {
            auto local = vector<T>{};

            {
                lock_guard lock(_mutex);
                _pending.swap(local);
            }

            for (const auto& evt : local)
            {
                handler(evt);
            }
        }

        /// @brief Returns the number of pending events.
        /// Acquires the mutex; intended for diagnostics only.
        [[nodiscard]] auto size() const noexcept -> size_t
        {
            lock_guard lock(_mutex);
            return _pending.size();
        }

        /// @brief Discard all pending events.
        auto clear() noexcept -> void
        {
            lock_guard lock(_mutex);
            _pending.clear();
        }

      private:
        mutable mutex _mutex;
        vector<T> _pending;
    };

} // namespace tempest::event

#endif // tempest_event_event_queue_hpp
