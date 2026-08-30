#ifndef tempest_window_handle_hpp
#define tempest_window_handle_hpp

#include <tempest/int.hpp>

namespace tempest
{
    /// \brief Strongly-typed handle representing an OS window managed by window_manager.
    struct window_handle
    {
        uint32_t id{0};

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return id != 0;
        }

        constexpr auto operator<=>(const window_handle&) const noexcept = default;
    };

    inline constexpr window_handle null_window_handle{0};

    template <typename T>
    struct hash;

    template <>
    struct hash<window_handle>
    {
        constexpr auto operator()(const window_handle& handle) const noexcept -> size_t
        {
            return static_cast<size_t>(handle.id);
        }
    };
} // namespace tempest

#endif // tempest_window_handle_hpp
