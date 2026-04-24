#ifndef tempest_event_event_dispatcher_hpp
#define tempest_event_event_dispatcher_hpp

/// @file event_dispatcher.hpp
/// @brief Typed event dispatcher with immediate listener invocation and queue fan-out.
///
/// An event_dispatcher<T> is the central broadcast point for events of type T. When publish()
/// is called, it:
///   1. Invokes all subscribed listener callables synchronously on the calling thread.
///   2. Enqueues the event into all subscribed event_queue<T> instances.
///
/// Listeners are stored in a slot_map for O(1) subscribe/unsubscribe with dense iteration.
/// Queue pointers are stored in a separate slot_map (non-owning — queues are owned by subsystems).
///
/// Subscription handles use generational keys: unsubscribing with a stale handle returns false
/// rather than corrupting state.
///
/// Non-copyable, non-moveable: address stability is assumed by the event_registry that owns
/// dispatchers, and by subsystems that hold references.
///
/// Thread safety: publish() is NOT internally synchronized. If subscribe/unsubscribe can race
/// with publish (e.g., hot-reload while game loop runs), external synchronization is required.
/// For the common pattern of subscribe-during-init / publish-during-loop, no lock is needed.

#include <tempest/event.hpp>
#include <tempest/event_queue.hpp>
#include <tempest/functional.hpp>
#include <tempest/slot_map.hpp>

namespace tempest::event
{
    /// @brief Broadcasts events of type T to listeners and queues.
    /// @tparam T Event type. Must satisfy the event_type concept.
    template <event_type T>
    class event_dispatcher
    {
      public:
        using handle_type = subscription_handle<T>;

        event_dispatcher() noexcept = default;

        event_dispatcher(const event_dispatcher&) = delete;
        event_dispatcher(event_dispatcher&&) = delete;
        event_dispatcher& operator=(const event_dispatcher&) = delete;
        event_dispatcher& operator=(event_dispatcher&&) = delete;

        ~event_dispatcher() = default;

        /// @brief Subscribe a listener callable.
        /// @param listener A callable accepting const T&. Stored as function<void(const T&)>.
        /// @return A subscription handle for later unsubscription.
        [[nodiscard]] auto subscribe(function<void(const T&)> listener) noexcept -> handle_type
        {
            auto key = _listeners.insert(tempest::move(listener));
            return handle_type{key};
        }

        /// @brief Unsubscribe a previously registered listener.
        /// @param handle The handle returned from subscribe().
        /// @return true if the listener was found and removed, false if the handle was stale or invalid.
        [[nodiscard]] auto unsubscribe(handle_type handle) noexcept -> bool
        {
            return _listeners.erase(handle.key);
        }

        /// @brief Subscribe an event queue for deferred processing.
        /// The dispatcher holds a non-owning pointer to the queue. The caller must ensure the queue
        /// outlives its subscription, or must call unsubscribe_queue() before the queue is destroyed.
        /// @param queue Reference to an event_queue<T> owned by the caller.
        /// @return A subscription handle for later unsubscription.
        [[nodiscard]] auto subscribe_queue(event_queue<T>& queue) noexcept -> handle_type
        {
            auto key = _queues.insert(&queue);
            return handle_type{key};
        }

        /// @brief Unsubscribe a previously registered event queue.
        /// @param handle The handle returned from subscribe_queue().
        /// @return true if the queue was found and removed, false if the handle was stale or invalid.
        [[nodiscard]] auto unsubscribe_queue(handle_type handle) noexcept -> bool
        {
            return _queues.erase(handle.key);
        }

        /// @brief Publish an event to all listeners and queues.
        /// Listeners are invoked synchronously on the calling thread. Queues receive
        /// the event via enqueue() (which internally acquires its own mutex).
        /// @param evt The event to publish.
        auto publish(const T& evt) noexcept -> void
        {
            for (const auto& listener : _listeners)
            {
                listener(evt);
            }

            for (auto* queue : _queues)
            {
                queue->enqueue(evt);
            }
        }

        /// @brief Returns the number of active listener subscriptions.
        [[nodiscard]] auto listener_count() const noexcept -> size_t
        {
            return _listeners.size();
        }

        /// @brief Returns the number of active queue subscriptions.
        [[nodiscard]] auto queue_count() const noexcept -> size_t
        {
            return _queues.size();
        }

      private:
        slot_map<function<void(const T&)>> _listeners;
        slot_map<event_queue<T>*> _queues;
    };

} // namespace tempest::event

#endif // tempest_event_event_dispatcher_hpp
