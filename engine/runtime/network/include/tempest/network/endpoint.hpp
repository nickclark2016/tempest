#ifndef tempest_network_endpoint_hpp
#define tempest_network_endpoint_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>

namespace tempest::network
{
    class TEMPEST_API endpoint
    {
      public:
        constexpr endpoint() noexcept = default;
        constexpr endpoint(uint32_t ipv4_address, uint16_t port) noexcept
            : _address{ipv4_address}, _port{port}
        {
        }

        constexpr endpoint(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint16_t port) noexcept
            : _address{(static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
                       (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d)},
              _port{port}
        {
        }

        [[nodiscard]] static constexpr auto loopback(uint16_t port) noexcept -> endpoint
        {
            // 127.0.0.1 -- Default loopback address
            // NOLINTNEXTLINE
            return {127, 0, 0, 1, port};
        }

        [[nodiscard]] static constexpr auto any(uint16_t port) noexcept -> endpoint
        {
            // 0.0.0.0 -- Default any address
            // NOLINTNEXTLINE
            return {0, 0, 0, 0, port};
        }

        [[nodiscard]] static auto parse(string_view str) noexcept -> optional<endpoint>;

        [[nodiscard]] constexpr auto address() const noexcept -> uint32_t
        {
            return _address;
        }

        [[nodiscard]] constexpr auto port() const noexcept -> uint16_t
        {
            return _port;
        }

        [[nodiscard]] auto to_string() const -> string;

        [[nodiscard]] friend constexpr auto operator==(const endpoint& lhs, const endpoint& rhs) noexcept -> bool
        {
            return lhs._address == rhs._address && lhs._port == rhs._port;
        }

        [[nodiscard]] friend constexpr auto operator!=(const endpoint& lhs, const endpoint& rhs) noexcept -> bool
        {
            return !(lhs == rhs);
        }

      private:
        uint32_t _address{0};
        uint16_t _port{0};
    };
} // namespace tempest::network

#endif // tempest_network_endpoint_hpp
