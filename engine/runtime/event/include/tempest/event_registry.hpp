#ifndef tempest_event_event_registry_hpp
#define tempest_event_event_registry_hpp

/// @file event_registry.hpp
/// @brief Type-erased registry that owns event dispatchers, one per event type.
///
/// The event_registry is the top-level owner of the event system. It lazily creates an
/// event_dispatcher<T> on first access for each event type T and holds it for the lifetime
/// of the registry. Subsystems access dispatchers via typed references:
///
///     auto& d = registry.dispatcher<MyEvent>();
///     d.subscribe(...);
///     d.publish(MyEvent{...});
///
/// Type erasure is implemented via a minimal channel_base with virtual destructor — the sole
/// use of runtime polymorphism in the event system, justified at the storage boundary.
/// Dispatchers themselves are fully template-based with no virtual dispatch.
///
/// Thread safety: dispatcher<T>() uses a mutex to guard lazy creation. Once all dispatcher
/// types are created (typically during initialization), subsequent calls are still guarded
/// but the lock is uncontended. If dispatcher creation is confined to a single-threaded
/// init phase, the lock cost is negligible.
///
/// Coroutine extension point: when coroutine-based task parallelism is added, each coroutine
/// or task can own its own event_queue<T> instances and register them with the relevant
/// dispatcher. The queue follows the coroutine's lifetime, not a thread's. drain() works
/// regardless of which thread resumes the coroutine — the queue's internal mutex ensures
/// correctness. No thread-local storage is needed.
///
/// Non-copyable, non-moveable: destroying the registry destroys all owned dispatchers.

#include <tempest/api.hpp>
#include <tempest/event_dispatcher.hpp>
#include <tempest/memory.hpp>
#include <tempest/mutex.hpp>
#include <tempest/vector.hpp>

namespace tempest::event
{
    /// @brief Owns one event_dispatcher<T> per event type, created on first access.
    class TEMPEST_API event_registry
    {
      public:
        event_registry() noexcept = default;

        event_registry(const event_registry&) = delete;
        event_registry(event_registry&&) = delete;
        auto operator=(const event_registry&) -> event_registry& = delete;
        auto operator=(event_registry&&) -> event_registry& = delete;

        ~event_registry() = default;

        /// @brief Get or create the dispatcher for event type T.
        /// Thread-safe: guarded by mutex for lazy creation.
        /// @tparam T Event type. Must satisfy the event_type concept.
        /// @return Reference to the dispatcher. Stable for the lifetime of this registry.
        template <event_type T>
        [[nodiscard]] auto dispatcher() noexcept -> event_dispatcher<T>&
        {
            // Fast path: check if already created. We scan linearly — the number of distinct
            // event types is expected to be small (tens, not thousands).
            // Lock is held for the full lookup+create to prevent races on first access.
            auto lock = lock_guard(_mutex);

            for (const auto& channel_ptr : _channels)
            {
                if (channel_ptr->type_id() == _type_id<T>())
                {
                    return static_cast<channel<T>*>(channel_ptr.get())->get();
                }
            }

            // Not found — create a new channel for this event type.
            auto created = make_unique<channel<T>>();
            auto& ref = created->get();
            _channels.push_back(tempest::move(created));
            return ref;
        }

      private:
        /// @brief Compile-time unique ID per event type, using function pointer identity.
        /// Avoids RTTI dependency.
        template <event_type T>
        static constexpr void _type_id_fn() noexcept
        {
        }

        template <event_type T>
        static constexpr auto _type_id() noexcept -> void (*)()
        {
            return &_type_id_fn<T>;
        }

        /// @brief Type-erased base for per-type dispatcher storage.
        /// Virtual destructor is the sole use of runtime polymorphism in the event system.
        struct channel_base
        {
            channel_base() = default;
            channel_base(const channel_base&) = delete;
            channel_base(channel_base&&) = delete;
            auto operator=(const channel_base&) -> channel_base& = delete;
            auto operator=(channel_base&&) -> channel_base& = delete;
            virtual ~channel_base() = default;

            [[nodiscard]] virtual auto type_id() const noexcept -> void (*)() = 0;
        };

        /// @brief Typed channel owning an event_dispatcher<T>.
        template <event_type T>
        struct channel final : channel_base
        {
            [[nodiscard]] auto get() noexcept -> event_dispatcher<T>&
            {
                return _dispatcher;
            }

            [[nodiscard]] auto type_id() const noexcept -> void (*)() override
            {
                return _type_id<T>();
            }

          private:
            event_dispatcher<T> _dispatcher;
        };

        mutable mutex _mutex;
        vector<unique_ptr<channel_base>> _channels;
    };

} // namespace tempest::event

#endif // tempest_event_event_registry_hpp
