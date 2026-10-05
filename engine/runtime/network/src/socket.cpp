#include <tempest/network/socket.hpp>

#include <tempest/utility.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

using native_socket_t = SOCKET;
constexpr native_socket_t invalid_native_socket = INVALID_SOCKET;

namespace
{
    auto set_socket_nonblocking(native_socket_t socket_handle) -> bool
    {
        auto mode = u_long{1};
        return ioctlsocket(socket_handle, FIONBIO, &mode) == 0;
    }

    auto close_native_socket(native_socket_t socket_handle) -> void
    {
        closesocket(socket_handle);
    }

    auto map_last_socket_error() -> tempest::network::socket_error
    {
        const auto error_code = WSAGetLastError();
        switch (error_code)
        {
            case WSAEWOULDBLOCK:
                return tempest::network::socket_error::would_block;
            case WSAEMSGSIZE:
                return tempest::network::socket_error::message_too_large;
            case WSAECONNRESET:
                return tempest::network::socket_error::connection_reset;
            case WSAENETUNREACH:
                return tempest::network::socket_error::network_unreachable;
            default:
                return tempest::network::socket_error::other;
        }
    }
} // namespace

#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using native_socket_t = int;
constexpr native_socket_t invalid_native_socket = -1;

namespace
{
    auto set_socket_nonblocking(native_socket_t socket_handle) -> bool
    {
        const auto flags = fcntl(socket_handle, F_GETFL, 0);
        if (flags == -1)
        {
            return false;
        }
        return fcntl(socket_handle, F_SETFL, flags | O_NONBLOCK) != -1;
    }

    auto close_native_socket(native_socket_t socket_handle) -> void
    {
        close(socket_handle);
    }

    auto map_last_socket_error() -> tempest::network::socket_error
    {
        switch (errno)
        {
            case EWOULDBLOCK:
#if defined(EAGAIN) && (EAGAIN != EWOULDBLOCK)
            case EAGAIN:
#endif
            case EINTR:
                return tempest::network::socket_error::would_block;
            case EMSGSIZE:
                return tempest::network::socket_error::message_too_large;
            case ECONNRESET:
                return tempest::network::socket_error::connection_reset;
            case ENETUNREACH:
                return tempest::network::socket_error::network_unreachable;
            default:
                return tempest::network::socket_error::other;
        }
    }
} // namespace
#endif

namespace tempest::network
{
    network_context::network_context()
    {
#ifdef _WIN32
        auto wsa_data = WSADATA{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0)
        {
            _initialized = true;
        }
#else
        signal(SIGPIPE, SIG_IGN);
        _initialized = true;
#endif
    }

    network_context::~network_context()
    {
        if (_initialized)
        {
#ifdef _WIN32
            WSACleanup();
#endif
            _initialized = false;
        }
    }

    network_context::network_context(network_context&& other) noexcept : _initialized{other._initialized}
    {
        other._initialized = false;
    }

    auto network_context::operator=(network_context&& other) noexcept -> network_context&
    {
        if (this != &other)
        {
            if (_initialized)
            {
#ifdef _WIN32
                WSACleanup();
#endif
            }
            _initialized = other._initialized;
            other._initialized = false;
        }
        return *this;
    }

    udp_socket::udp_socket() noexcept = default;

    udp_socket::~udp_socket()
    {
        close();
    }

    udp_socket::udp_socket(udp_socket&& other) noexcept
        : _handle{other._handle},
          _bound_endpoint{other._bound_endpoint},
          _last_error{other._last_error}
    {
        other._handle = invalid_socket_handle;
        other._bound_endpoint = endpoint{};
        other._last_error = socket_error::none;
    }

    auto udp_socket::operator=(udp_socket&& other) noexcept -> udp_socket&
    {
        if (this != &other)
        {
            close();
            _handle = other._handle;
            _bound_endpoint = other._bound_endpoint;
            _last_error = other._last_error;
            other._handle = invalid_socket_handle;
            other._bound_endpoint = endpoint{};
            other._last_error = socket_error::none;
        }
        return *this;
    }

    auto udp_socket::open() -> bool
    {
        if (is_open())
        {
            _last_error = socket_error::none;
            return true;
        }

#ifdef _WIN32
        auto wsa_data = WSADATA{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        {
            _last_error = socket_error::other;
            return false;
        }
#else
        signal(SIGPIPE, SIG_IGN);
#endif

        const auto socket_descriptor = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_descriptor == invalid_native_socket)
        {
            _last_error = map_last_socket_error();
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        if (!set_socket_nonblocking(socket_descriptor))
        {
            _last_error = map_last_socket_error();
            close_native_socket(socket_descriptor);
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

#ifdef _WIN32
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
        auto new_behavior = DWORD{0};
        auto bytes_returned = DWORD{0};
        WSAIoctl(socket_descriptor, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior), nullptr, 0, &bytes_returned,
                 nullptr, nullptr);
#endif

        _handle = static_cast<uintptr_t>(socket_descriptor);
        _last_error = socket_error::none;
        return true;
    }

    auto udp_socket::close() -> void
    {
        if (is_open())
        {
            close_native_socket(static_cast<native_socket_t>(_handle));
            _handle = invalid_socket_handle;
            _bound_endpoint = endpoint{};
#ifdef _WIN32
            WSACleanup();
#endif
        }
        _last_error = socket_error::none;
    }

    auto udp_socket::bind(const endpoint& ep) -> bool // NOLINT
    {
        if (!is_open())
        {
            if (!open())
            {
                return false;
            }
        }

        auto bind_address = sockaddr_in{};
        bind_address.sin_family = AF_INET;
        bind_address.sin_port = htons(ep.port());
        bind_address.sin_addr.s_addr = htonl(ep.address());

        const auto bind_result = ::bind(static_cast<native_socket_t>(_handle),
                                        reinterpret_cast<const sockaddr*>(&bind_address), sizeof(bind_address));
        if (bind_result != 0)
        {
            _last_error = map_last_socket_error();
            return false;
        }

        _bound_endpoint = ep;
        _last_error = socket_error::none;
        return true;
    }

    auto udp_socket::send_to(const endpoint& destination, span<const byte> data) -> size_t
    {
        if (!is_open())
        {
            _last_error = socket_error::other;
            return 0;
        }
        if (data.empty())
        {
            _last_error = socket_error::none;
            return 0;
        }

        auto target_address = sockaddr_in{};
        target_address.sin_family = AF_INET;
        target_address.sin_port = htons(destination.port());
        target_address.sin_addr.s_addr = htonl(destination.address());

        const auto bytes_to_send = static_cast<int>(data.size());
        const auto sent_result =
            ::sendto(static_cast<native_socket_t>(_handle), reinterpret_cast<const char*>(data.data()), bytes_to_send,
                     0, reinterpret_cast<const sockaddr*>(&target_address), sizeof(target_address));

        if (sent_result < 0)
        {
            _last_error = map_last_socket_error();
            return 0;
        }

        _last_error = socket_error::none;
        return static_cast<size_t>(sent_result);
    }

    auto udp_socket::receive_from(endpoint& out_sender, span<byte> buffer) -> size_t
    {
        if (!is_open())
        {
            _last_error = socket_error::other;
            return 0;
        }
        if (buffer.empty())
        {
            _last_error = socket_error::none;
            return 0;
        }

        auto sender_address = sockaddr_in{};
        auto sender_length = static_cast<socklen_t>(sizeof(sender_address));

        const auto buffer_size = static_cast<int>(buffer.size());
        const auto received_result =
            ::recvfrom(static_cast<native_socket_t>(_handle), reinterpret_cast<char*>(buffer.data()), buffer_size, 0,
                       reinterpret_cast<sockaddr*>(&sender_address), &sender_length);

        if (received_result < 0)
        {
            _last_error = map_last_socket_error();
            return 0;
        }

        _last_error = socket_error::none;
        out_sender = endpoint(ntohl(sender_address.sin_addr.s_addr), ntohs(sender_address.sin_port));
        return static_cast<size_t>(received_result);
    }

    auto udp_socket::is_open() const noexcept -> bool
    {
        return _handle != invalid_socket_handle;
    }

    auto udp_socket::local_endpoint() const noexcept -> optional<endpoint>
    {
        if (!is_open())
        {
            return nullopt;
        }

        auto socket_address = sockaddr_in{};
        auto address_length = static_cast<socklen_t>(sizeof(socket_address));
        if (getsockname(static_cast<native_socket_t>(_handle), reinterpret_cast<sockaddr*>(&socket_address),
                        &address_length) != 0)
        {
            return nullopt;
        }

        return endpoint(ntohl(socket_address.sin_addr.s_addr), ntohs(socket_address.sin_port));
    }

    auto udp_socket::last_error() const noexcept -> socket_error
    {
        return _last_error;
    }
} // namespace tempest::network
