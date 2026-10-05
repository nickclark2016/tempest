#ifndef tempest_network_socket_hpp
#define tempest_network_socket_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/network/endpoint.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>

namespace tempest::network
{
    inline constexpr uintptr_t invalid_socket_handle = ~static_cast<uintptr_t>(0);

    /// @brief Diagnostic socket errors for granular transmission and reception failure visibility.
    enum class socket_error : uint8_t
    {
        none = 0,
        would_block,
        message_too_large,
        connection_reset,
        network_unreachable,
        other,
    };

    /// @brief Engine-compliant RAII network context managing platform socket subsystem initialization.
    class TEMPEST_API network_context
    {
      public:
        network_context();
        ~network_context();

        network_context(const network_context&) = delete;
        auto operator=(const network_context&) -> network_context& = delete;
        network_context(network_context&& other) noexcept;
        auto operator=(network_context&& other) noexcept -> network_context&;

        [[nodiscard]] auto is_initialized() const noexcept -> bool
        {
            return _initialized;
        }

      private:
        bool _initialized = false;
    };

    class TEMPEST_API udp_socket
    {
      public:
        udp_socket() noexcept;
        ~udp_socket();

        udp_socket(const udp_socket&) = delete;
        auto operator=(const udp_socket&) -> udp_socket& = delete;
        udp_socket(udp_socket&& other) noexcept;
        auto operator=(udp_socket&& other) noexcept -> udp_socket&;

        [[nodiscard]] auto open() -> bool;
        auto close() -> void;
        [[nodiscard]] auto bind(const endpoint& ep) -> bool;

        [[nodiscard]] auto send_to(const endpoint& destination, span<const byte> data) -> size_t;
        [[nodiscard]] auto receive_from(endpoint& out_sender, span<byte> buffer) -> size_t;

        [[nodiscard]] auto is_open() const noexcept -> bool;
        [[nodiscard]] auto local_endpoint() const noexcept -> optional<endpoint>;
        [[nodiscard]] auto last_error() const noexcept -> socket_error;

      private:
        uintptr_t _handle = invalid_socket_handle;
        endpoint _bound_endpoint;
        socket_error _last_error = socket_error::none;
    };
} // namespace tempest::network

#endif // tempest_network_socket_hpp
