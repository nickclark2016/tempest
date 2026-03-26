#ifndef tempest_event_event_hpp
#define tempest_event_event_hpp

/// @file event.hpp
/// @brief Core event system concepts, traits, and subscription handle types.
///
/// Events in this system are trivially copyable, trivially destructible value types.
/// Listeners are callables accepting a const reference to an event type.
/// Subscription handles are generational keys backed by slot_map, enabling O(1) subscribe/unsubscribe
/// with use-after-free detection via generation mismatch.

#include <tempest/slot_map.hpp>
#include <tempest/type_traits.hpp>

namespace tempest::event
{
    /// @brief Concept constraining types that can be used as events.
    /// Events must be trivially copyable and trivially destructible to support lock-free
    /// queue operations (memcpy-safe) and to avoid destructor overhead during drain.
    template <typename T>
    concept event_type = is_trivially_copyable_v<T> && is_trivially_destructible_v<T>;

    /// @brief Opaque handle returned from subscribe/subscribe_queue operations.
    /// Wraps a slot_map key with generational validation. A stale handle (one whose generation
    /// no longer matches the slot) will fail to unsubscribe gracefully rather than corrupting state.
    /// @tparam T The event type this subscription is associated with.
    template <event_type T>
    struct subscription_handle
    {
        using key_type = typename slot_map<int>::key_type; // matches slot_map's key type (uint64_t on 64-bit)

        key_type key;

        constexpr auto operator==(const subscription_handle& rhs) const noexcept -> bool = default;
        constexpr auto operator!=(const subscription_handle& rhs) const noexcept -> bool = default;
    };

    /// @brief Sentinel value representing an invalid or uninitialized subscription.
    /// @tparam T The event type.
    template <event_type T>
    inline constexpr subscription_handle<T> null_subscription{slot_map_traits<typename subscription_handle<T>::key_type>::empty};

} // namespace tempest::event

#endif // tempest_event_event_hpp
