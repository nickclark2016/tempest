#ifndef tempest_profiler_web_server_hpp
#define tempest_profiler_web_server_hpp

#include <tempest/api.hpp>
#include <tempest/atomic.hpp>
#include <tempest/int.hpp>
#include <tempest/mutex.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/profiler/websocket.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/thread.hpp>
#include <tempest/vector.hpp>

#include <chrono>

namespace tempest::profiler
{
    struct web_server_config
    {
        string host{"127.0.0.1"};
        uint16_t port{8080};
        uint16_t max_port_attempts{10};
        bool enable_live_stream{true};
    };

    class TEMPEST_API web_server
    {
      public:
        explicit web_server(profiler_session& session, web_server_config config = {});
        ~web_server();

        web_server(const web_server&) = delete;
        web_server& operator=(const web_server&) = delete;
        web_server(web_server&&) noexcept = delete;
        web_server& operator=(web_server&&) noexcept = delete;

        auto start() -> void;
        auto stop() -> void;
        [[nodiscard]] auto is_running() const noexcept -> bool;
        [[nodiscard]] auto get_bound_port() const noexcept -> uint16_t;
        [[nodiscard]] auto get_server_url() const noexcept -> string;
        [[nodiscard]] auto connected_client_count() const noexcept -> size_t;

        auto broadcast_frame(span<const byte> frame_payload) -> void;
        auto broadcast_text(string_view text) -> void;
        auto broadcast_telemetry(const telemetry_frame& frame) -> void;

      private:
        auto _server_loop() -> void;
        auto _handle_http_request(int64_t client_socket, string_view request_header) -> void;
        auto _handle_websocket_connection(int64_t client_socket, string_view ws_key) -> void;
        auto _process_websocket_frame(int64_t client_socket, const ws_message& msg) -> bool;
        auto _handle_control_command(int64_t client_socket, string_view command_str) -> void;
        auto _close_client_socket(int64_t client_socket) -> void;

        struct pending_connection
        {
            int64_t socket{-1};
            std::chrono::steady_clock::time_point connected_at{};
        };

        profiler_session& _session;
        web_server_config _config;
        atomic<bool> _running{false};
        atomic<uint16_t> _bound_port{0};
        atomic<int64_t> _server_socket{-1};
        thread _worker_thread{};

        vector<pending_connection> _pending_clients{};
        vector<int64_t> _ws_clients{};
        mutable mutex _clients_mutex{};
    };
} // namespace tempest::profiler

#endif // tempest_profiler_web_server_hpp
