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
#include <winsock2.h>
#include <ws2tcpip.h>
using native_socket_t = SOCKET;
constexpr native_socket_t invalid_sock = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
using native_socket_t = int;
constexpr native_socket_t invalid_sock = -1;
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
            static auto guard = winsock_guard{};
            (void)guard;
        }

        auto close_socket(native_socket_t s) -> void
        {
            closesocket(s);
        }
#else
        auto ensure_sockets_initialized() -> void
        {
        }

        auto close_socket(native_socket_t s) -> void
        {
            close(s);
        }
#endif

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

        auto str_contains(string_view sv, string_view target) -> bool
        {
            return find_sub(sv, target) != static_cast<size_t>(-1);
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

        _server_socket = static_cast<int64_t>(bound_sock);
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

        if (_server_socket != -1)
        {
            close_socket(static_cast<native_socket_t>(_server_socket));
            _server_socket = -1;
        }

        {
            lock_guard<mutex> lock(_clients_mutex);
            for (const auto client : _ws_clients)
            {
                close_socket(static_cast<native_socket_t>(client));
            }
            _ws_clients.clear();
        }

        if (_worker_thread.joinable())
        {
            _worker_thread.join();
        }

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
        lock_guard<mutex> lock(_clients_mutex);
        return _ws_clients.size();
    }

    auto web_server::broadcast_frame(span<const byte> frame_payload) -> void
    {
        if (!_running.load() || frame_payload.empty())
        {
            return;
        }

        const auto frame = encode_websocket_frame(ws_opcode::binary, frame_payload);
        auto dead_clients = vector<int64_t>{};

        {
            lock_guard<mutex> lock(_clients_mutex);
            for (const auto client : _ws_clients)
            {
                const auto res = send(static_cast<native_socket_t>(client), reinterpret_cast<const char*>(frame.data()),
                                      static_cast<int>(frame.size()), 0);
                if (res < 0)
                {
                    dead_clients.push_back(client);
                }
            }

            for (const auto dead : dead_clients)
            {
                for (auto it = _ws_clients.begin(); it != _ws_clients.end(); ++it)
                {
                    if (*it == dead)
                    {
                        _ws_clients.erase(it);
                        break;
                    }
                }
                close_socket(static_cast<native_socket_t>(dead));
            }
        }
    }

    auto web_server::broadcast_text(string_view text) -> void
    {
        if (!_running.load() || text.empty())
        {
            return;
        }

        const auto frame = encode_websocket_frame(
            ws_opcode::text, span<const byte>{reinterpret_cast<const byte*>(text.data()), text.size()});
        auto dead_clients = vector<int64_t>{};

        {
            lock_guard<mutex> lock(_clients_mutex);
            for (const auto client : _ws_clients)
            {
                const auto res = send(static_cast<native_socket_t>(client), reinterpret_cast<const char*>(frame.data()),
                                      static_cast<int>(frame.size()), 0);
                if (res < 0)
                {
                    dead_clients.push_back(client);
                }
            }

            for (const auto dead : dead_clients)
            {
                for (auto it = _ws_clients.begin(); it != _ws_clients.end(); ++it)
                {
                    if (*it == dead)
                    {
                        _ws_clients.erase(it);
                        break;
                    }
                }
                close_socket(static_cast<native_socket_t>(dead));
            }
        }
    }

    auto web_server::broadcast_telemetry(const telemetry_frame& frame) -> void
    {
        const auto json_str = serialize_telemetry_frame_json(frame);
        broadcast_text(string_view{json_str.data(), json_str.size()});
    }

    auto web_server::_server_loop() -> void
    {
        while (_running.load())
        {
            auto read_fds = fd_set{};
            FD_ZERO(&read_fds);

            if (_server_socket == -1)
            {
                break;
            }

            const auto server_sock = static_cast<native_socket_t>(_server_socket);
            FD_SET(server_sock, &read_fds);
            auto max_fd = server_sock;

            auto active_clients = vector<int64_t>{};
            {
                lock_guard<mutex> lock(_clients_mutex);
                for (const auto c : _ws_clients)
                {
                    const auto sock = static_cast<native_socket_t>(c);
                    FD_SET(sock, &read_fds);
                    if (sock > max_fd)
                    {
                        max_fd = sock;
                    }
                }
                active_clients = _ws_clients;
            }

            auto tv = timeval{};
            tv.tv_sec = 0;
            tv.tv_usec = 50000; // 50 ms

            const auto activity = select(static_cast<int>(max_fd + 1), &read_fds, nullptr, nullptr, &tv);
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
                const auto client_sock = accept(server_sock, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

                if (client_sock != invalid_sock)
                {
                    auto nodelay = int{1};
                    setsockopt(client_sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay),
                               sizeof(nodelay));

                    char buffer[4096];
                    const auto bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
                    if (bytes > 0)
                    {
                        buffer[bytes] = '\0';
                        const auto req_view = string_view{buffer, static_cast<size_t>(bytes)};
                        const auto upgrade = find_header_value(req_view, "Upgrade");
                        const auto sec_key = find_header_value(req_view, "Sec-WebSocket-Key");

                        if (!upgrade.empty() && !sec_key.empty())
                        {
                            _handle_websocket_connection(static_cast<int64_t>(client_sock), sec_key);
                        }
                        else
                        {
                            _handle_http_request(static_cast<int64_t>(client_sock), req_view);
                        }
                    }
                    else
                    {
                        close_socket(client_sock);
                    }
                }
            }

            // Read from active WebSocket clients
            for (const auto c : active_clients)
            {
                const auto client_sock = static_cast<native_socket_t>(c);
                if (FD_ISSET(client_sock, &read_fds))
                {
                    char buffer[4096];
                    const auto bytes = recv(client_sock, buffer, sizeof(buffer), 0);
                    if (bytes <= 0)
                    {
                        _close_client_socket(c);
                    }
                    else
                    {
                        const auto decoded = decode_websocket_frame(
                            span<const byte>{reinterpret_cast<const byte*>(buffer), static_cast<size_t>(bytes)});
                        if (decoded)
                        {
                            _process_websocket_frame(c, *decoded);
                        }
                        else
                        {
                            if (decoded.error() == ws_error::invalid_frame ||
                                decoded.error() == ws_error::connection_closed)
                            {
                                _close_client_socket(c);
                            }
                        }
                    }
                }
            }
        }
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

        send(static_cast<native_socket_t>(client_socket), response.data(), static_cast<int>(response.size()), 0);
        close_socket(static_cast<native_socket_t>(client_socket));
    }

    auto web_server::_handle_websocket_connection(int64_t client_socket, string_view ws_key) -> void
    {
        const auto accept_key = compute_websocket_accept_key(ws_key);
        auto response = std::string{};
        std::format_to(std::back_inserter(response),
                       "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: {}\r\n"
                       "\r\n",
                       accept_key.c_str());

        const auto res =
            send(static_cast<native_socket_t>(client_socket), response.data(), static_cast<int>(response.size()), 0);
        if (res >= 0)
        {
            lock_guard<mutex> lock(_clients_mutex);
            _ws_clients.push_back(client_socket);
        }
        else
        {
            close_socket(static_cast<native_socket_t>(client_socket));
        }
    }

    auto web_server::_process_websocket_frame(int64_t client_socket, const ws_message& msg) -> void
    {
        if (msg.opcode == ws_opcode::ping)
        {
            const auto pong = encode_websocket_frame(ws_opcode::pong, msg.payload);
            send(static_cast<native_socket_t>(client_socket), reinterpret_cast<const char*>(pong.data()),
                 static_cast<int>(pong.size()), 0);
        }
        else if (msg.opcode == ws_opcode::close)
        {
            const auto close_f = encode_websocket_frame(ws_opcode::close, {});
            send(static_cast<native_socket_t>(client_socket), reinterpret_cast<const char*>(close_f.data()),
                 static_cast<int>(close_f.size()), 0);
            _close_client_socket(client_socket);
        }
        else if (msg.opcode == ws_opcode::text)
        {
            const auto text = string_view{reinterpret_cast<const char*>(msg.payload.data()), msg.payload.size()};
            _handle_control_command(client_socket, text);
        }
    }

    auto web_server::_handle_control_command(int64_t client_socket, string_view command_str) -> void
    {
        auto resp = std::string{};

        if (str_contains(command_str, "start_capture"))
        {
            _session.set_enabled(true);
            resp = "{\"type\":\"response\",\"command\":\"start_capture\",\"status\":\"ok\",\"recording\":true}";
        }
        else if (str_contains(command_str, "stop_capture"))
        {
            _session.set_enabled(false);
            const auto capture = create_capture_from_session(_session);
            std::format_to(std::back_inserter(resp),
                           "{{\"type\":\"response\",\"command\":\"stop_capture\",\"status\":\"ok\",\"recording\":false,"
                           "\"tracks\":{},\"metrics\":{}}}",
                           capture.tracks.size(), capture.metrics.size());
        }
        else if (str_contains(command_str, "query_stats"))
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
        else if (str_contains(command_str, "get_snapshot"))
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
        send(static_cast<native_socket_t>(client_socket), reinterpret_cast<const char*>(frame.data()),
             static_cast<int>(frame.size()), 0);
    }

    auto web_server::_close_client_socket(int64_t client_socket) -> void
    {
        {
            lock_guard<mutex> lock(_clients_mutex);
            for (auto it = _ws_clients.begin(); it != _ws_clients.end(); ++it)
            {
                if (*it == client_socket)
                {
                    _ws_clients.erase(it);
                    break;
                }
            }
        }
        close_socket(static_cast<native_socket_t>(client_socket));
    }
} // namespace tempest::profiler
