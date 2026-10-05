#include <gtest/gtest.h>

#include <tempest/network/endpoint.hpp>
#include <tempest/network/socket.hpp>

#include <tempest/array.hpp>
#include <tempest/chrono.hpp>
#include <tempest/span.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/thread.hpp>

namespace tempest::network::tests
{
    // =========================================================================
    // SECTION: Endpoint Parsing and Formatting Tests
    // =========================================================================

    /// @brief Verifies parsing string addresses and formatting to_string round-trip.
    TEST(socket_tests, endpoint_parse_and_format)
    {
        // 1. Setup: Define known endpoints
        const auto expected_loopback = endpoint(127, 0, 0, 1, 8080);
        const auto expected_gateway = endpoint(192, 168, 1, 1, 27015);
        const auto expected_broadcast = endpoint(255, 255, 255, 255, 65535);
        const auto expected_any = endpoint(0, 0, 0, 0, 0);

        // 2. Act: Parse string representations
        const auto parsed_loopback = endpoint::parse("127.0.0.1:8080");
        const auto parsed_gateway = endpoint::parse("192.168.1.1:27015");
        const auto parsed_broadcast = endpoint::parse("255.255.255.255:65535");
        const auto parsed_any = endpoint::parse("0.0.0.0:0");

        // 3. Assert: Parsing returns expected endpoint values
        ASSERT_TRUE(parsed_loopback.has_value());
        EXPECT_EQ(parsed_loopback.value(), expected_loopback);
        EXPECT_EQ(parsed_loopback.value().to_string(), "127.0.0.1:8080");

        ASSERT_TRUE(parsed_gateway.has_value());
        EXPECT_EQ(parsed_gateway.value(), expected_gateway);
        EXPECT_EQ(parsed_gateway.value().to_string(), "192.168.1.1:27015");

        ASSERT_TRUE(parsed_broadcast.has_value());
        EXPECT_EQ(parsed_broadcast.value(), expected_broadcast);
        EXPECT_EQ(parsed_broadcast.value().to_string(), "255.255.255.255:65535");

        ASSERT_TRUE(parsed_any.has_value());
        EXPECT_EQ(parsed_any.value(), expected_any);
        EXPECT_EQ(parsed_any.value().to_string(), "0.0.0.0:0");

        // Invalid inputs
        EXPECT_FALSE(endpoint::parse("").has_value());
        EXPECT_FALSE(endpoint::parse("127.0.0.1:").has_value());
        EXPECT_FALSE(endpoint::parse("256.0.0.1:80").has_value());
        EXPECT_FALSE(endpoint::parse("127.0.0.1:65536").has_value());
        EXPECT_FALSE(endpoint::parse("invalid_endpoint").has_value());
        EXPECT_FALSE(endpoint::parse("127.0.0.1:8080extra").has_value());
        EXPECT_FALSE(endpoint::parse("1.2.3").has_value());
        EXPECT_FALSE(endpoint::parse("1.2.3.4.5:80").has_value());
    }

    // =========================================================================
    // SECTION: UDP Socket Lifecycle Tests
    // =========================================================================

    /// @brief Verifies opening and binding socket to loopback with port 0, and closing.
    TEST(socket_tests, socket_open_bind_close)
    {
        // 1. Setup: Create unopened UDP socket
        auto test_socket = udp_socket{};
        EXPECT_FALSE(test_socket.is_open());
        EXPECT_FALSE(test_socket.local_endpoint().has_value());

        // 2. Act: Open socket and bind to loopback with ephemeral port
        const auto open_success = test_socket.open();
        EXPECT_TRUE(open_success);
        EXPECT_TRUE(test_socket.is_open());

        const auto bind_success = test_socket.bind(endpoint::loopback(0));
        EXPECT_TRUE(bind_success);

        // 3. Assert: Local endpoint has valid loopback address and non-zero bound port
        const auto bound_endpoint_opt = test_socket.local_endpoint();
        ASSERT_TRUE(bound_endpoint_opt.has_value());
        EXPECT_EQ(bound_endpoint_opt.value().address(), endpoint::loopback(0).address());
        EXPECT_GT(bound_endpoint_opt.value().port(), 0);

        test_socket.close();
        EXPECT_FALSE(test_socket.is_open());
        EXPECT_FALSE(test_socket.local_endpoint().has_value());
    }

    /// @brief Verifies receive_from returns 0 when no data is available without blocking.
    TEST(socket_tests, socket_non_blocking_empty_drain)
    {
        // 1. Setup: Create bound UDP socket on loopback
        auto drain_socket = udp_socket{};
        ASSERT_TRUE(drain_socket.bind(endpoint::loopback(0)));
        auto receive_buffer = array<byte, 64>{};
        auto sender_endpoint = endpoint{};

        // 2. Act: Attempt to receive when socket queue is empty
        const auto received_bytes = drain_socket.receive_from(
            sender_endpoint,
            span<byte>{receive_buffer.data(), receive_buffer.size()});

        // 3. Assert: Immediately returns 0 bytes without blocking
        EXPECT_EQ(received_bytes, 0);
    }

    /// @brief Verifies two non-blocking sockets exchanging packets over 127.0.0.1.
    TEST(socket_tests, socket_loopback_send_receive)
    {
        // 1. Setup: Create and bind server socket, create client socket
        auto server_socket = udp_socket{};
        ASSERT_TRUE(server_socket.bind(endpoint::loopback(0)));
        const auto server_endpoint_opt = server_socket.local_endpoint();
        ASSERT_TRUE(server_endpoint_opt.has_value());
        const auto server_endpoint = server_endpoint_opt.value();

        auto client_socket = udp_socket{};
        ASSERT_TRUE(client_socket.open());

        const auto payload = array<byte, 8>{
            byte{0xDE}, byte{0xAD}, byte{0xBE}, byte{0xEF},
            byte{0xCA}, byte{0xFE}, byte{0xBA}, byte{0xBE}
        };

        // 2. Act: Client sends payload to server
        const auto sent_bytes = client_socket.send_to(
            server_endpoint,
            span<const byte>{payload.data(), payload.size()});
        EXPECT_EQ(sent_bytes, payload.size());

        // Drain / receive packet from server socket with bounded retry
        auto received_buffer = array<byte, 64>{};
        auto sender_endpoint = endpoint{};
        auto received_bytes = size_t{0};

        for (auto attempt_index = size_t{0}; attempt_index < 100 && received_bytes == 0; ++attempt_index)
        {
            received_bytes = server_socket.receive_from(
                sender_endpoint,
                span<byte>{received_buffer.data(), received_buffer.size()});
            if (received_bytes == 0)
            {
                this_thread::sleep_for(chrono::milliseconds{1});
            }
        }

        // 3. Assert: Packet was received intact with valid sender endpoint
        EXPECT_EQ(received_bytes, payload.size());
        for (auto byte_index = size_t{0}; byte_index < payload.size(); ++byte_index)
        {
            EXPECT_EQ(received_buffer[byte_index], payload[byte_index]);
        }
        EXPECT_EQ(sender_endpoint.address(), endpoint::loopback(0).address());
        EXPECT_GT(sender_endpoint.port(), 0);
    }

    // =========================================================================
    // SECTION: Socket Error Diagnostics and Context Lifecycle Tests
    // =========================================================================

    /// @brief Verifies that socket operations update last_error() accurately, distinguishing would_block
    /// and resetting to none on success.
    TEST(socket_tests, socket_error_diagnostic_tracking)
    {
        // 1. Setup: Create server socket bound to loopback and client socket
        auto server_socket = udp_socket{};
        ASSERT_TRUE(server_socket.bind(endpoint::loopback(0)));
        EXPECT_EQ(server_socket.last_error(), socket_error::none);

        auto client_socket = udp_socket{};
        ASSERT_TRUE(client_socket.open());
        EXPECT_EQ(client_socket.last_error(), socket_error::none);

        // 2. Act: Attempt receive on empty non-blocking server socket
        auto receive_buffer = array<byte, 64>{};
        auto sender_endpoint = endpoint{};
        const auto received_empty = server_socket.receive_from(
            sender_endpoint,
            span<byte>{receive_buffer.data(), receive_buffer.size()});

        // 3. Assert: Empty non-blocking receive returns 0 and sets last_error to would_block
        EXPECT_EQ(received_empty, 0);
        EXPECT_EQ(server_socket.last_error(), socket_error::would_block);

        // 4. Act: Client sends a datagram to server
        const auto server_ep = server_socket.local_endpoint().value();
        const auto payload = array<byte, 4>{byte{1}, byte{2}, byte{3}, byte{4}};
        const auto sent_bytes = client_socket.send_to(server_ep, span<const byte>{payload.data(), payload.size()});
        EXPECT_EQ(sent_bytes, 4);
        EXPECT_EQ(client_socket.last_error(), socket_error::none);

        // Server receives the datagram with bounded retry
        auto received_bytes = size_t{0};
        for (auto attempt_index = size_t{0}; attempt_index < 100 && received_bytes == 0; ++attempt_index)
        {
            received_bytes = server_socket.receive_from(
                sender_endpoint,
                span<byte>{receive_buffer.data(), receive_buffer.size()});
            if (received_bytes == 0)
            {
                this_thread::sleep_for(chrono::milliseconds{1});
            }
        }

        // 5. Assert: Successful receive resets last_error to none
        EXPECT_EQ(received_bytes, 4);
        EXPECT_EQ(server_socket.last_error(), socket_error::none);
    }

    /// @brief Verifies that network_context initializes and cleans up the platform socket subsystem
    /// following RAII semantics and supports move construction and assignment.
    TEST(socket_tests, network_context_raii_lifecycle)
    {
        // 1. Setup & Act: Construct primary network_context
        auto context = network_context{};
        EXPECT_TRUE(context.is_initialized());

        // 2. Act: Move construct
        auto moved_context = tempest::move(context);
        EXPECT_TRUE(moved_context.is_initialized());
        EXPECT_FALSE(context.is_initialized());

        // 3. Act: Move assign
        auto target_context = network_context{};
        EXPECT_TRUE(target_context.is_initialized());
        target_context = tempest::move(moved_context);
        EXPECT_TRUE(target_context.is_initialized());
        EXPECT_FALSE(moved_context.is_initialized());

        // 4. Assert: Scope exit destroys target_context cleanly
    }
} // namespace tempest::network::tests
