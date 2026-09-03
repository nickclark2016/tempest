#include "web_assets.hpp"
#include <tempest/algorithm.hpp>
#include <tempest/profiler/capture.hpp>
#include <tempest/profiler/serialization.hpp>
#include <tempest/profiler/statistics.hpp>
#include <tempest/profiler/web_server.hpp>

#include <cctype>
#include <cstring>
#include <format>
#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef FD_SETSIZE
#define FD_SETSIZE 1024
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using native_socket_t = SOCKET;
constexpr native_socket_t invalid_sock = INVALID_SOCKET;
constexpr auto nosignal_flag = int{0};
constexpr auto shutdown_send = SD_SEND;
constexpr auto shutdown_both = SD_BOTH;
#else
#include <arpa/inet.h>
#include <csignal>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using native_socket_t = int;
constexpr native_socket_t invalid_sock = -1;
#if defined(MSG_NOSIGNAL)
constexpr auto nosignal_flag = MSG_NOSIGNAL;
#else
constexpr auto nosignal_flag = int{0};
#endif
constexpr auto shutdown_send = SHUT_WR;
constexpr auto shutdown_both = SHUT_RDWR;
#endif

namespace tempest::profiler
{
    namespace
    {
#if defined(_WIN32)
        struct winsock_guard
        {
            winsock_guard()
            {
                auto wsa_data = WSADATA{};
                WSAStartup(MAKEWORD(2, 2), &wsa_data);
            }
            ~winsock_guard()
            {
                WSACleanup();
            }
        };

        auto ensure_sockets_initialized() -> void
        {
            [[maybe_unused]] static auto guard = winsock_guard{};
        }

        auto close_socket(native_socket_t s) -> void
        {
            closesocket(s);
        }

        auto set_socket_nonblocking(native_socket_t s) -> void
        {
            auto mode = u_long{1};
            ioctlsocket(s, FIONBIO, &mode);
        }
#else
        auto ensure_sockets_initialized() -> void
        {
            [[maybe_unused]] static auto initialized = []() {
                signal(SIGPIPE, SIG_IGN);
                return true;
            }();
        }

        auto close_socket(native_socket_t s) -> void
        {
            close(s);
        }

        auto set_socket_nonblocking(native_socket_t s) -> void
        {
            auto flags = fcntl(s, F_GETFL, 0);
            fcntl(s, F_SETFL, flags | O_NONBLOCK);
        }
#endif

        auto graceful_close_socket(native_socket_t s, int how = shutdown_both) -> void
        {
            if (s == invalid_sock)
            {
                return;
            }
            shutdown(s, how);
            if (how == shutdown_send)
            {
                char drain_buf[512];
                while (recv(s, drain_buf, sizeof(drain_buf), 0) > 0)
                {
                }
            }
            close_socket(s);
        }

        auto send_all_nonblocking(native_socket_t sock, const char* data, size_t total_size) -> bool
        {
            auto bytes_sent = size_t{0};
            while (bytes_sent < total_size)
            {
                const auto chunk_to_send = static_cast<int>(tempest::min(total_size - bytes_sent, size_t{65536}));
                const auto res = send(sock, data + bytes_sent, chunk_to_send, nosignal_flag);
                if (res > 0)
                {
                    bytes_sent += static_cast<size_t>(res);
                }
                else if (res < 0)
                {
#if defined(_WIN32)
                    const auto err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK)
                    {
                        auto write_fds = fd_set{};
                        FD_ZERO(&write_fds);
                        FD_SET(sock, &write_fds);
                        auto tv = timeval{.tv_sec = 0, .tv_usec = 2000};
                        if (select(0, nullptr, &write_fds, nullptr, &tv) > 0)
                        {
                            continue;
                        }
                        return false;
                    }
#else
                    const auto err = errno;
                    if (err == EWOULDBLOCK || err == EAGAIN)
                    {
                        if (sock >= FD_SETSIZE)
                        {
                            return false;
                        }
                        auto write_fds = fd_set{};
                        FD_ZERO(&write_fds);
                        FD_SET(sock, &write_fds);
                        auto tv = timeval{.tv_sec = 0, .tv_usec = 2000};
                        if (select(static_cast<int>(sock + 1), nullptr, &write_fds, nullptr, &tv) > 0)
                        {
                            continue;
                        }
                        return false;
                    }
#endif
                    return false;
                }
                else
                {
                    return false;
                }
            }
            return true;
        }

        auto find_sub(string_view sv, string_view target, size_t pos = 0) -> size_t
        {
            if (target.empty())
            {
                return pos <= sv.size() ? pos : static_cast<size_t>(-1);
            }
            if (pos + target.size() > sv.size())
            {
                return static_cast<size_t>(-1);
            }
            for (auto i = pos; i + target.size() <= sv.size(); ++i)
            {
                auto match = true;
                for (auto j = size_t{0}; j < target.size(); ++j)
                {
                    if (sv[i + j] != target[j])
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    return i;
                }
            }
            return static_cast<size_t>(-1);
        }

        auto find_char(string_view sv, char ch, size_t pos = 0) -> size_t
        {
            for (auto i = pos; i < sv.size(); ++i)
            {
                if (sv[i] == ch)
                {
                    return i;
                }
            }
            return static_cast<size_t>(-1);
        }

        auto sub_view(string_view sv, size_t pos, size_t count = static_cast<size_t>(-1)) -> string_view
        {
            if (pos >= sv.size())
            {
                return string_view{};
            }
            const auto available = sv.size() - pos;
            const auto actual_count = (count < available) ? count : available;
            return string_view{sv.data() + pos, actual_count};
        }

        auto extract_command_name(string_view text) -> string_view
        {
            // Try extracting "command" property: "command" : "value"
            const auto cmd_key_pos = find_sub(text, "\"command\"");
            if (cmd_key_pos != static_cast<size_t>(-1))
            {
                const auto colon = find_char(text, ':', cmd_key_pos + 9);
                if (colon != static_cast<size_t>(-1))
                {
                    auto val_start = colon + 1;
                    while (val_start < text.size() && (text[val_start] == ' ' || text[val_start] == '\t' ||
                                                       text[val_start] == '\r' || text[val_start] == '\n'))
                    {
                        ++val_start;
                    }
                    if (val_start < text.size() && text[val_start] == '"')
                    {
                        ++val_start;
                        const auto val_end = find_char(text, '"', val_start);
                        if (val_end != static_cast<size_t>(-1))
                        {
                            return sub_view(text, val_start, val_end - val_start);
                        }
                    }
                }
            }

            // Fallback: trim quotes, brackets, and whitespace
            auto start = size_t{0};
            while (start < text.size() && (text[start] == ' ' || text[start] == '\t' || text[start] == '"' ||
                                           text[start] == '{' || text[start] == '\r' || text[start] == '\n'))
            {
                ++start;
            }
            auto end = text.size();
            while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '"' ||
                                   text[end - 1] == '}' || text[end - 1] == '\r' || text[end - 1] == '\n'))
            {
                --end;
            }
            return sub_view(text, start, end - start);
        }

        auto find_header_value(string_view header_str, string_view key_name) -> string_view
        {
            auto pos = size_t{0};
            while (pos < header_str.size())
            {
                auto line_end = find_sub(header_str, "\r\n", pos);
                if (line_end == static_cast<size_t>(-1))
                {
                    line_end = find_char(header_str, '\n', pos);
                }
                if (line_end == static_cast<size_t>(-1))
                {
                    line_end = header_str.size();
                }

                const auto line = sub_view(header_str, pos, line_end - pos);
                const auto colon = find_char(line, ':');
                if (colon != static_cast<size_t>(-1))
                {
                    const auto header_key = sub_view(line, 0, colon);
                    if (header_key.size() == key_name.size())
                    {
                        auto match = true;
                        for (auto i = size_t{0}; i < key_name.size(); ++i)
                        {
                            if (std::tolower(static_cast<unsigned char>(header_key[i])) !=
                                std::tolower(static_cast<unsigned char>(key_name[i])))
                            {
                                match = false;
                                break;
                            }
                        }
                        if (match)
                        {
                            auto val = sub_view(line, colon + 1);
                            auto vpos = size_t{0};
                            while (vpos < val.size() && (val[vpos] == ' ' || val[vpos] == '\t'))
                            {
                                ++vpos;
                            }
                            return sub_view(val, vpos);
                        }
                    }
                }

                if (line_end == header_str.size())
                {
                    break;
                }
                pos = (line_end < header_str.size() && header_str[line_end] == '\r' &&
                       line_end + 1 < header_str.size() && header_str[line_end + 1] == '\n')
                          ? line_end + 2
                          : line_end + 1;
            }
            return string_view{};
        }

        auto extract_request_path(string_view header_str) -> string_view
        {
            const auto first_space = find_char(header_str, ' ');
            if (first_space == static_cast<size_t>(-1))
            {
                return "/";
            }
            const auto second_space = find_char(header_str, ' ', first_space + 1);
            auto path = (second_space == static_cast<size_t>(-1))
                            ? sub_view(header_str, first_space + 1)
                            : sub_view(header_str, first_space + 1, second_space - first_space - 1);
            const auto question_mark = find_char(path, '?');
            if (question_mark != static_cast<size_t>(-1))
            {
                path = sub_view(path, 0, question_mark);
            }
            return path;
        }
    } // namespace

    web_server::web_server(profiler_session& session, web_server_config config)
        : _session{session}, _config{tempest::move(config)}
    {
    }

    web_server::~web_server()
    {
        stop();
    }

    auto web_server::start() -> void
    {
        if (_running.load())
        {
            return;
        }

        ensure_sockets_initialized();

        const auto max_attempts = static_cast<uint16_t>(max(static_cast<uint16_t>(1), _config.max_port_attempts));
        auto bound_port = uint16_t{0};
        auto bound_sock = invalid_sock;

        for (auto i = uint16_t{0}; i < max_attempts; ++i)
        {
            const auto port = static_cast<uint16_t>(_config.port + i);
            const auto s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s == invalid_sock)
            {
                continue;
            }

#if defined(_WIN32)
            auto opt = int{1};
            setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
            auto opt = int{1};
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#endif

            auto addr = sockaddr_in{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, _config.host.c_str(), &addr.sin_addr);

            if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0)
            {
                if (listen(s, SOMAXCONN) == 0)
                {
                    set_socket_nonblocking(s);
                    bound_port = port;
                    bound_sock = s;
                    break;
                }
            }

            close_socket(s);
        }

        if (bound_sock == invalid_sock)
        {
            _running.store(false);
            _bound_port.store(0);
            return;
        }

        if (_worker_thread.joinable())
        {
            _worker_thread.join();
        }

        _server_socket.store(static_cast<int64_t>(bound_sock));
        _bound_port.store(bound_port);
        _running.store(true);

        _worker_thread = thread([this]() { _server_loop(); });
    }

    auto web_server::stop() -> void
    {
        if (!_running.exchange(false))
        {
            return;
        }

        if (_worker_thread.joinable())
        {
            _worker_thread.join();
        }

        const auto s = _server_socket.exchange(-1);
        if (s != -1)
        {
            close_socket(static_cast<native_socket_t>(s));
        }

        {
            auto lock = lock_guard{_clients_mutex};
            for (const auto& client : _ws_clients)
            {
                close_socket(static_cast<native_socket_t>(client.socket));
            }
            _ws_clients.clear();
        }

        for (const auto& pending : _pending_clients)
        {
            close_socket(static_cast<native_socket_t>(pending.socket));
        }
        _pending_clients.clear();

        _bound_port.store(0);
    }

    auto web_server::is_running() const noexcept -> bool
    {
        return _running.load();
    }

    auto web_server::get_bound_port() const noexcept -> uint16_t
    {
        return _bound_port.load();
    }

    auto web_server::get_server_url() const noexcept -> string
    {
        const auto port = _bound_port.load();
        if (port == 0)
        {
            return string{};
        }
        auto str = std::format("http://{}:{}", _config.host.c_str(), port);
        return string{str.data(), str.size()};
    }

    auto web_server::connected_client_count() const noexcept -> size_t
    {
        auto lock = lock_guard{_clients_mutex};
        return _ws_clients.size();
    }

    auto web_server::broadcast_frame(span<const byte> frame_payload) -> void
    {
        if (!_running.load() || frame_payload.empty())
        {
            return;
        }

        auto lock = lock_guard{_clients_mutex};
        if (_ws_clients.empty())
        {
            return;
        }

        const auto frame = encode_websocket_frame(ws_opcode::binary, frame_payload);

        for (auto it = _ws_clients.begin(); it != _ws_clients.end();)
        {
            const auto client = it->socket;
            if (!send_all_nonblocking(static_cast<native_socket_t>(client), reinterpret_cast<const char*>(frame.data()),
                                      frame.size()))
            {
                close_socket(static_cast<native_socket_t>(client));
                it = _ws_clients.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    auto web_server::broadcast_text(string_view text) -> void
    {
        if (!_running.load() || text.empty())
        {
            return;
        }

        auto lock = lock_guard{_clients_mutex};
        if (_ws_clients.empty())
        {
            return;
        }

        const auto frame = encode_websocket_frame(
            ws_opcode::text, span<const byte>{reinterpret_cast<const byte*>(text.data()), text.size()});

        for (auto it = _ws_clients.begin(); it != _ws_clients.end();)
        {
            const auto client = it->socket;
            if (!send_all_nonblocking(static_cast<native_socket_t>(client), reinterpret_cast<const char*>(frame.data()),
                                      frame.size()))
            {
                close_socket(static_cast<native_socket_t>(client));
                it = _ws_clients.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    auto web_server::broadcast_telemetry(const telemetry_frame& frame) -> void
    {
        if (!_running.load())
        {
            return;
        }

        {
            auto lock = lock_guard{_clients_mutex};
            if (_ws_clients.empty())
            {
                return;
            }
        }

        const auto json_str = serialize_telemetry_frame_json(frame);
        broadcast_text(string_view{json_str.data(), json_str.size()});
    }

    auto web_server::_server_loop() -> void
    {
        auto process_pending_data = [this](int64_t sock, string& rx_buf) -> bool {
            const auto header_end = find_sub(string_view{rx_buf.data(), rx_buf.size()}, "\r\n\r\n");
            if (header_end == static_cast<size_t>(-1))
            {
                if (rx_buf.size() > 65536)
                {
                    close_socket(static_cast<native_socket_t>(sock));
                    return true;
                }
                return false;
            }

            const auto req_view = string_view{rx_buf.data(), header_end + 4};
            const auto upgrade = find_header_value(req_view, "Upgrade");
            const auto sec_key = find_header_value(req_view, "Sec-WebSocket-Key");

            const auto leftover_start = header_end + 4;
            const auto leftover_size = rx_buf.size() - leftover_start;
            const auto leftover_span =
                span<const byte>{reinterpret_cast<const byte*>(rx_buf.data() + leftover_start), leftover_size};

            if (!upgrade.empty() && !sec_key.empty())
            {
                _handle_websocket_connection(sock, sec_key, leftover_span);
            }
            else
            {
                _handle_http_request(sock, req_view);
            }
            return true;
        };

        while (_running.load())
        {
            auto read_fds = fd_set{};
            FD_ZERO(&read_fds);

            const auto s = _server_socket.load();
            if (s == -1)
            {
                break;
            }

            const auto server_sock = static_cast<native_socket_t>(s);
            FD_SET(server_sock, &read_fds);
            auto max_fd = server_sock;

            {
                auto lock = lock_guard{_clients_mutex};
                for (const auto& c : _ws_clients)
                {
                    const auto sock = static_cast<native_socket_t>(c.socket);
#if !defined(_WIN32)
                    if (sock >= FD_SETSIZE)
                    {
                        continue;
                    }
#endif
                    FD_SET(sock, &read_fds);
                    if (sock > max_fd)
                    {
                        max_fd = sock;
                    }
                }
            }

            for (const auto& pending : _pending_clients)
            {
                const auto sock = static_cast<native_socket_t>(pending.socket);
#if !defined(_WIN32)
                if (sock >= FD_SETSIZE)
                {
                    continue;
                }
#endif
                FD_SET(sock, &read_fds);
                if (sock > max_fd)
                {
                    max_fd = sock;
                }
            }

            auto tv = timeval{};
            tv.tv_sec = 0;
            tv.tv_usec = 50000; // 50 ms

#if defined(_WIN32)
            const auto nfds = int{0};
#else
            const auto nfds = static_cast<int>(max_fd + 1);
#endif
            const auto activity = select(nfds, &read_fds, nullptr, nullptr, &tv);
            if (activity < 0 || !_running.load())
            {
                continue;
            }
            if (activity == 0)
            {
                continue;
            }

            // Accept new connections
            if (FD_ISSET(server_sock, &read_fds))
            {
                auto client_addr = sockaddr_in{};
                auto addr_len = static_cast<socklen_t>(sizeof(client_addr));
                while (true)
                {
                    {
                        auto lock = lock_guard{_clients_mutex};
                        if (_ws_clients.size() + _pending_clients.size() + 1 >= FD_SETSIZE)
                        {
                            break;
                        }
                    }

                    const auto client_sock = accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
                    if (client_sock == invalid_sock)
                    {
                        break;
                    }

#if !defined(_WIN32)
                    if (client_sock >= FD_SETSIZE)
                    {
                        close_socket(client_sock);
                        break;
                    }
#endif

                    set_socket_nonblocking(client_sock);
                    auto nodelay = int{1};
                    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay),
                               sizeof(nodelay));

                    char buffer[4096];
                    const auto bytes = recv(client_sock, buffer, sizeof(buffer), 0);
                    if (bytes > 0)
                    {
                        auto rx = string{buffer, static_cast<size_t>(bytes)};
                        if (!process_pending_data(static_cast<int64_t>(client_sock), rx))
                        {
                            _pending_clients.push_back(pending_connection{
                                .socket = static_cast<int64_t>(client_sock),
                                .connected_at = std::chrono::steady_clock::now(),
                                .rx_buffer = tempest::move(rx),
                            });
                        }
                    }
                    else if (bytes < 0)
                    {
#if defined(_WIN32)
                        const auto err = WSAGetLastError();
                        if (err == WSAEWOULDBLOCK)
#else
                        const auto err = errno;
                        if (err == EWOULDBLOCK || err == EAGAIN)
#endif
                        {
                            _pending_clients.push_back(pending_connection{
                                .socket = static_cast<int64_t>(client_sock),
                                .connected_at = std::chrono::steady_clock::now(),
                                .rx_buffer = {},
                            });
                            continue;
                        }
                        close_socket(client_sock);
                    }
                    else
                    {
                        close_socket(client_sock);
                    }
                }
            }

            // Read from pending handshake clients
            const auto now = std::chrono::steady_clock::now();
            for (auto it = _pending_clients.begin(); it != _pending_clients.end();)
            {
                const auto client_sock = static_cast<native_socket_t>(it->socket);
                auto remove_pending = false;

                if (FD_ISSET(client_sock, &read_fds))
                {
                    char buffer[4096];
                    const auto bytes = recv(client_sock, buffer, sizeof(buffer), 0);
                    if (bytes > 0)
                    {
                        it->rx_buffer.append(buffer, static_cast<size_t>(bytes));
                        if (process_pending_data(it->socket, it->rx_buffer))
                        {
                            remove_pending = true;
                        }
                    }
                    else if (bytes == 0)
                    {
                        close_socket(client_sock);
                        remove_pending = true;
                    }
                    else
                    {
#if defined(_WIN32)
                        const auto err = WSAGetLastError();
                        if (err != WSAEWOULDBLOCK)
#else
                        const auto err = errno;
                        if (err != EWOULDBLOCK && err != EAGAIN)
#endif
                        {
                            close_socket(client_sock);
                            remove_pending = true;
                        }
                    }
                }
                else
                {
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - it->connected_at).count() >= 5)
                    {
                        close_socket(client_sock);
                        remove_pending = true;
                    }
                }

                if (remove_pending)
                {
                    it = _pending_clients.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // Read from active WebSocket clients
            {
                auto lock = lock_guard{_clients_mutex};
                for (auto it = _ws_clients.begin(); it != _ws_clients.end();)
                {
                    const auto c = it->socket;
                    const auto client_sock = static_cast<native_socket_t>(c);
                    if (FD_ISSET(client_sock, &read_fds) || !it->rx_buffer.empty())
                    {
                        auto should_close = false;

                        if (FD_ISSET(client_sock, &read_fds))
                        {
                            char buffer[4096];
                            const auto bytes = recv(client_sock, buffer, sizeof(buffer), 0);
                            if (bytes <= 0)
                            {
                                if (bytes < 0)
                                {
#if defined(_WIN32)
                                    const auto err = WSAGetLastError();
                                    if (err != WSAEWOULDBLOCK)
                                    {
                                        should_close = true;
                                    }
#else
                                    const auto err = errno;
                                    if (err != EWOULDBLOCK && err != EAGAIN)
                                    {
                                        should_close = true;
                                    }
#endif
                                }
                                else
                                {
                                    should_close = true;
                                }
                            }
                            else
                            {
                                it->rx_buffer.insert(it->rx_buffer.end(), reinterpret_cast<const byte*>(buffer),
                                                     reinterpret_cast<const byte*>(buffer) + bytes);
                            }
                        }

                        // Decode all complete frames from rx_buffer
                        while (!it->rx_buffer.empty() && !should_close)
                        {
                            auto bytes_consumed = size_t{0};
                            const auto decoded = decode_websocket_frame(
                                span<const byte>{it->rx_buffer.data(), it->rx_buffer.size()}, bytes_consumed);
                            if (decoded)
                            {
                                if (!decoded->is_masked)
                                {
                                    // RFC 6455 Section 5.1: The server MUST close the connection upon receiving a frame
                                    // that is not masked
                                    should_close = true;
                                    break;
                                }

                                if (!_process_websocket_frame(c, *decoded))
                                {
                                    should_close = true;
                                    break;
                                }
                                it->rx_buffer.erase(it->rx_buffer.begin(), it->rx_buffer.begin() + bytes_consumed);
                            }
                            else
                            {
                                if (decoded.error() == ws_error::incomplete_frame)
                                {
                                    break;
                                }
                                should_close = true;
                                break;
                            }
                        }

                        if (should_close)
                        {
                            close_socket(client_sock);
                            it = _ws_clients.erase(it);
                            continue;
                        }
                    }
                    ++it;
                }
            }
        }

        for (const auto& pending : _pending_clients)
        {
            close_socket(static_cast<native_socket_t>(pending.socket));
        }
        _pending_clients.clear();
    }

    auto web_server::_handle_http_request(int64_t client_socket, string_view request_header) -> void
    {
        const auto path = extract_request_path(request_header);
        auto response = std::string{};

        if (path == "/" || path == "/index.html")
        {
            const auto content = get_embedded_index_html();
            std::format_to(std::back_inserter(response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=utf-8\r\n"
                           "Content-Length: {}\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           content.size());
            response.append(content.data(), content.size());
        }
        else if (path == "/app.js")
        {
            const auto content = get_embedded_app_js();
            std::format_to(std::back_inserter(response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/javascript; charset=utf-8\r\n"
                           "Content-Length: {}\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           content.size());
            response.append(content.data(), content.size());
        }
        else if (path == "/styles.css")
        {
            const auto content = get_embedded_styles_css();
            std::format_to(std::back_inserter(response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/css; charset=utf-8\r\n"
                           "Content-Length: {}\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           content.size());
            response.append(content.data(), content.size());
        }
        else if (path == "/status" || path == "/health")
        {
            constexpr const char* status_json = "{\"status\":\"ok\",\"profiler_running\":true}";
            const auto json_len = std::strlen(status_json);
            std::format_to(std::back_inserter(response),
                           "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json; charset=utf-8\r\n"
                           "Content-Length: {}\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "{}",
                           json_len, status_json);
        }
        else
        {
            constexpr const char* not_found = "Not Found";
            const auto nf_len = std::strlen(not_found);
            std::format_to(std::back_inserter(response),
                           "HTTP/1.1 404 Not Found\r\n"
                           "Content-Type: text/plain; charset=utf-8\r\n"
                           "Content-Length: {}\r\n"
                           "Connection: close\r\n"
                           "\r\n"
                           "{}",
                           nf_len, not_found);
        }

        send_all_nonblocking(static_cast<native_socket_t>(client_socket), response.data(), response.size());
        graceful_close_socket(static_cast<native_socket_t>(client_socket), shutdown_send);
    }

    auto web_server::_handle_websocket_connection(int64_t client_socket, string_view ws_key,
                                                  span<const byte> initial_rx) -> void
    {
        const auto native_sock = static_cast<native_socket_t>(client_socket);
        set_socket_nonblocking(native_sock);
        auto buf_size = int{1024 * 1024}; // 1 MB send and receive buffers
        setsockopt(native_sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));
        setsockopt(native_sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&buf_size), sizeof(buf_size));
        const auto accept_key = compute_websocket_accept_key(ws_key);
        auto response = std::string{};
        std::format_to(std::back_inserter(response),
                       "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: {}\r\n"
                       "\r\n",
                       accept_key.c_str());

        const auto res = send(static_cast<native_socket_t>(client_socket), response.data(),
                              static_cast<int>(response.size()), nosignal_flag);
        if (res >= 0)
        {
            auto initial_buf = vector<byte>{};
            if (!initial_rx.empty())
            {
                initial_buf.assign(initial_rx.begin(), initial_rx.end());
            }
            auto lock = lock_guard{_clients_mutex};
            _ws_clients.push_back(websocket_client{
                .socket = client_socket,
                .rx_buffer = tempest::move(initial_buf),
            });
        }
        else
        {
            close_socket(static_cast<native_socket_t>(client_socket));
        }
    }

    auto web_server::_process_websocket_frame(int64_t client_socket, const ws_message& msg) -> bool
    {
        if (msg.opcode == ws_opcode::ping)
        {
            const auto pong = encode_websocket_frame(ws_opcode::pong, msg.payload);
            return send_all_nonblocking(static_cast<native_socket_t>(client_socket),
                                        reinterpret_cast<const char*>(pong.data()), pong.size());
        }
        else if (msg.opcode == ws_opcode::close)
        {
            const auto close_f = encode_websocket_frame(ws_opcode::close, {});
            send_all_nonblocking(static_cast<native_socket_t>(client_socket),
                                 reinterpret_cast<const char*>(close_f.data()), close_f.size());
            return false;
        }
        else if (msg.opcode == ws_opcode::text)
        {
            const auto text = string_view{reinterpret_cast<const char*>(msg.payload.data()), msg.payload.size()};
            return _handle_control_command(client_socket, text);
        }
        return true;
    }

    auto web_server::_handle_control_command(int64_t client_socket, string_view command_str) -> bool
    {
        const auto cmd = extract_command_name(command_str);
        auto resp = std::string{};

        if (cmd == "start_capture")
        {
            _session.set_enabled(true);
            resp = "{\"type\":\"response\",\"command\":\"start_capture\",\"status\":\"ok\",\"recording\":true}";
        }
        else if (cmd == "stop_capture")
        {
            _session.set_enabled(false);
            const auto capture = create_capture_from_session(_session);
            std::format_to(std::back_inserter(resp),
                           "{{\"type\":\"response\",\"command\":\"stop_capture\",\"status\":\"ok\",\"recording\":false,"
                           "\"tracks\":{},\"metrics\":{}}}",
                           capture.tracks.size(), capture.metrics.size());
        }
        else if (cmd == "query_stats")
        {
            const auto capture = create_capture_from_session(_session);
            const auto stats = compute_all_zone_statistics(capture);

            resp = "{\"type\":\"stats\",\"zone_count\":";
            std::format_to(std::back_inserter(resp), "{},\"zones\":[", stats.size());
            for (auto i = size_t{0}; i < stats.size(); ++i)
            {
                if (i > 0)
                {
                    resp += ",";
                }
                const auto& s = stats[i];
                auto zname_str = std::string(s.zone_name.data(), s.zone_name.size());
                std::format_to(std::back_inserter(resp),
                               "{{\"name\":\"{}\",\"count\":{},\"mean_ns\":{:.2f},\"min_ns\":{:.2f},\"max_ns\":{:.2f},"
                               "\"p50_ns\":{:.2f},\"p90_ns\":{:.2f},\"p95_ns\":{:.2f},\"p99_ns\":{:.2f},"
                               "\"std_deviation_ns\":{:.2f}}}",
                               zname_str, s.count, s.mean_ns, s.min_ns, s.max_ns, s.p50_ns, s.p90_ns, s.p95_ns,
                               s.p99_ns, s.std_deviation_ns);
            }
            resp += "]}";
        }
        else if (cmd == "get_snapshot")
        {
            const auto capture = create_capture_from_session(_session);
            const auto chrome_json = export_chrome_trace_json_string(capture);
            resp = std::string(chrome_json.data(), chrome_json.size());
        }
        else
        {
            resp = "{\"type\":\"error\",\"message\":\"unknown_command\"}";
        }

        const auto frame = encode_websocket_frame(
            ws_opcode::text, span<const byte>{reinterpret_cast<const byte*>(resp.data()), resp.size()});
        return send_all_nonblocking(static_cast<native_socket_t>(client_socket),
                                    reinterpret_cast<const char*>(frame.data()), frame.size());
    }

    auto web_server::_close_client_socket(int64_t client_socket) -> void
    {
        {
            auto lock = lock_guard{_clients_mutex};
            for (auto it = _ws_clients.begin(); it != _ws_clients.end(); ++it)
            {
                if (it->socket == client_socket)
                {
                    _ws_clients.erase(it);
                    break;
                }
            }
        }
        graceful_close_socket(static_cast<native_socket_t>(client_socket), shutdown_both);
    }
} // namespace tempest::profiler
