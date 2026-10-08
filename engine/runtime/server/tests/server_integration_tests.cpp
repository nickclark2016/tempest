#include <gtest/gtest.h>

#include <tempest/array.hpp>
#include <tempest/chrono.hpp>
#include <tempest/network/connection.hpp>
#include <tempest/network/endpoint.hpp>
#include <tempest/network/protocol.hpp>
#include <tempest/network/socket.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/physics/character_protocol.hpp>
#include <tempest/server/server_context.hpp>
#include <tempest/span.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::server::tests
{
    // =========================================================================
    // SECTION: Single-Client Connection & Handshake
    // =========================================================================

    /// @brief Verifies that a client connecting to server_context receives connect_accepted,
    /// receives an allocated session ID and slot, and causes the server to spawn a character.
    TEST(server_integration_tests, single_client_handshake)
    {
        // 1. Setup: Headless server on loopback port
        auto config = server_config{
            .port = 17777,
            .create_ground_plane = true,
        };
        auto server = server_context{config};
        ASSERT_TRUE(server.initialize());

        auto net_ctx = network::network_context{};
        auto client_socket = network::udp_socket{};
        ASSERT_TRUE(client_socket.open());
        ASSERT_TRUE(client_socket.bind(network::endpoint::loopback(0)));
        const auto server_ep = network::endpoint::loopback(config.port);

        // 2. Act: Send connect_request datagram to server
        auto writer = network::bit_writer{};
        network::write_connect_request(writer, network::connect_request_payload{
            .client_version = 1,
            .client_nonce = 12345ULL,
        });
        writer.flush();

        auto client_conn = network::connection{0, server_ep};
        const auto req_packet = client_conn.build_packet(network::packet_type::connect_request,
                                                         writer.data(), chrono::steady_clock::now());
        const auto sent_bytes = client_socket.send_to(server_ep, span<const byte>{req_packet.data(), req_packet.size()});
        ASSERT_GT(sent_bytes, 0U);

        // Server processes inbound datagrams
        server.poll_network_packets();

        // 3. Assert: Server registered session
        ASSERT_EQ(server.sessions().size(), 1U);
        EXPECT_EQ(server.sessions()[0].slot, 0U);
        EXPECT_NE(server.sessions()[0].session_id, 0ULL);
        EXPECT_TRUE(server.sessions()[0].character_entity != ecs::null);

        // Client receives connect_accepted datagram
        auto recv_buf = array<byte, 512>{};
        auto sender = network::endpoint{};
        const auto read_bytes = client_socket.receive_from(sender, span<byte>{recv_buf.data(), recv_buf.size()});
        ASSERT_GT(read_bytes, sizeof(network::packet_header));

        auto incoming_opt = client_conn.process_packet(span<const byte>{recv_buf.data(), read_bytes},
                                                       chrono::steady_clock::now());
        ASSERT_TRUE(incoming_opt.has_value());
        EXPECT_EQ(incoming_opt->header.type, static_cast<uint8_t>(network::packet_type::connect_accepted));

        auto accept_reader = network::bit_reader{incoming_opt->payload};
        const auto accepted_opt = network::read_connect_accepted(accept_reader);
        ASSERT_TRUE(accepted_opt.has_value());
        EXPECT_EQ(accepted_opt->session_id, server.sessions()[0].session_id);
        EXPECT_EQ(accepted_opt->player_slot, 0U);
    }

    // =========================================================================
    // SECTION: Multi-Client Staggered Spawning
    // =========================================================================

    /// @brief Verifies that multiple concurrent clients receive unique session IDs, consecutive slots,
    /// and staggered spawn coordinates along the X axis.
    TEST(server_integration_tests, multi_client_staggered_spawning)
    {
        // 1. Setup
        auto config = server_config{
            .port = 17778,
            .create_ground_plane = true,
        };
        auto server = server_context{config};
        ASSERT_TRUE(server.initialize());

        auto net_ctx = network::network_context{};
        auto client1 = network::udp_socket{};
        ASSERT_TRUE(client1.open());
        ASSERT_TRUE(client1.bind(network::endpoint::loopback(0)));

        auto client2 = network::udp_socket{};
        ASSERT_TRUE(client2.open());
        ASSERT_TRUE(client2.bind(network::endpoint::loopback(0)));

        const auto server_ep = network::endpoint::loopback(config.port);

        // 2. Act: Send connect requests from both clients
        auto writer1 = network::bit_writer{};
        network::write_connect_request(writer1, network::connect_request_payload{.client_nonce = 100ULL});
        writer1.flush();
        auto conn1 = network::connection{0, server_ep};
        const auto p1 = conn1.build_packet(network::packet_type::connect_request, writer1.data(), chrono::steady_clock::now());
        const auto s1 = client1.send_to(server_ep, span<const byte>{p1.data(), p1.size()});
        ASSERT_GT(s1, 0U);

        auto writer2 = network::bit_writer{};
        network::write_connect_request(writer2, network::connect_request_payload{.client_nonce = 200ULL});
        writer2.flush();
        auto conn2 = network::connection{0, server_ep};
        const auto p2 = conn2.build_packet(network::packet_type::connect_request, writer2.data(), chrono::steady_clock::now());
        const auto s2 = client2.send_to(server_ep, span<const byte>{p2.data(), p2.size()});
        ASSERT_GT(s2, 0U);

        // Server processes datagrams
        server.poll_network_packets();

        // 3. Assert
        ASSERT_EQ(server.sessions().size(), 2U);
        EXPECT_EQ(server.sessions()[0].slot, 0U);
        EXPECT_EQ(server.sessions()[1].slot, 1U);
        EXPECT_NE(server.sessions()[0].session_id, server.sessions()[1].session_id);

        const auto* const tx1 = server.get_registry().try_get<ecs::transform_component>(server.sessions()[0].character_entity);
        const auto* const tx2 = server.get_registry().try_get<ecs::transform_component>(server.sessions()[1].character_entity);
        ASSERT_NE(tx1, nullptr);
        ASSERT_NE(tx2, nullptr);

        EXPECT_FLOAT_EQ(tx1->position().x, 0.0F);
        EXPECT_FLOAT_EQ(tx2->position().x, 2.0F);
    }

    // =========================================================================
    // SECTION: Authoritative Simulation & Snapshot Broadcasting
    // =========================================================================

    /// @brief Verifies that client input drives authoritative character movement on the server
    /// and that broadcast snapshots replicate motion back to clients.
    TEST(server_integration_tests, authoritative_movement_and_snapshot_broadcast)
    {
        // 1. Setup
        auto config = server_config{
            .port = 17779,
            .create_ground_plane = true,
        };
        auto server = server_context{config};
        ASSERT_TRUE(server.initialize());

        auto net_ctx = network::network_context{};
        auto client_socket = network::udp_socket{};
        ASSERT_TRUE(client_socket.open());
        ASSERT_TRUE(client_socket.bind(network::endpoint::loopback(0)));
        const auto server_ep = network::endpoint::loopback(config.port);

        // Handshake
        auto req_writer = network::bit_writer{};
        network::write_connect_request(req_writer, network::connect_request_payload{.client_nonce = 777ULL});
        req_writer.flush();
        auto client_conn = network::connection{0, server_ep};
        const auto req_bytes = client_conn.build_packet(network::packet_type::connect_request, req_writer.data(), chrono::steady_clock::now());
        const auto sent_handshake = client_socket.send_to(server_ep, span<const byte>{req_bytes.data(), req_bytes.size()});
        ASSERT_GT(sent_handshake, 0U);

        server.poll_network_packets();
        ASSERT_EQ(server.sessions().size(), 1U);

        auto buf = array<byte, 1024>{};
        auto sender = network::endpoint{};
        const auto read_accepted = client_socket.receive_from(sender, span<byte>{buf.data(), buf.size()});
        ASSERT_GT(read_accepted, 0U);

        // 2. Act: Client streams input moving forward (+Z) over 30 ticks (0.5s at 60Hz)
        constexpr auto delta = 1.0 / 60.0;
        for (auto t = 0; t < 30; ++t)
        {
            const auto cmd = network::user_cmd{
                .tick = static_cast<uint32_t>(t + 1),
                .forward_move = 1.0F,
                .right_move = 0.0F,
                .view_yaw = 0.0F,
                .buttons = network::user_button_none,
            };
            auto cmd_writer = network::bit_writer{};
            const auto cmds = array<network::user_cmd, 1>{cmd};
            network::write_input_packet(cmd_writer, server.sessions()[0].session_id, static_cast<uint32_t>(t + 1), 0, 0,
                                        span<const network::user_cmd>{cmds.data(), cmds.size()});
            cmd_writer.flush();
            const auto sent_input = client_socket.send_to(server_ep, cmd_writer.data());
            ASSERT_GT(sent_input, 0U);

            server.poll_network_packets();
            server.tick_simulation(chrono::duration<double>{delta});
        }

        // Server broadcasts snapshot
        server.broadcast_snapshots();

        // 3. Assert: Client receives snapshot
        const auto snap_bytes = client_socket.receive_from(sender, span<byte>{buf.data(), buf.size()});
        ASSERT_GT(snap_bytes, sizeof(network::packet_header));

        auto snap_incoming = client_conn.process_packet(span<const byte>{buf.data(), snap_bytes}, chrono::steady_clock::now());
        ASSERT_TRUE(snap_incoming.has_value());
        EXPECT_EQ(snap_incoming->header.type, static_cast<uint8_t>(network::packet_type::snapshot));

        auto snap_reader = network::bit_reader{snap_incoming->payload};
        auto snapshot_packet = physics::world_snapshot_packet{};
        ASSERT_TRUE(physics::read_world_snapshot(snap_reader, snapshot_packet));

        ASSERT_EQ(snapshot_packet.players.size(), 1U);
        EXPECT_EQ(snapshot_packet.players[0].session_id, server.sessions()[0].session_id);

        // Character should have advanced forward along +Z
        EXPECT_GT(snapshot_packet.players[0].snapshot.position().z, 0.5F);
        EXPECT_TRUE(snapshot_packet.players[0].snapshot.is_grounded);
    }

    // =========================================================================
    // SECTION: Client Disconnect & Session Despawn
    // =========================================================================

    /// @brief Verifies that receiving a disconnect message cleanly despawns the player character
    /// from physics simulation and removes the session from the server.
    TEST(server_integration_tests, client_disconnect_despawn)
    {
        // 1. Setup
        auto config = server_config{
            .port = 17780,
            .create_ground_plane = true,
        };
        auto server = server_context{config};
        ASSERT_TRUE(server.initialize());

        auto net_ctx = network::network_context{};
        auto client_socket = network::udp_socket{};
        ASSERT_TRUE(client_socket.open());
        ASSERT_TRUE(client_socket.bind(network::endpoint::loopback(0)));
        const auto server_ep = network::endpoint::loopback(config.port);

        // Connect
        auto req_writer = network::bit_writer{};
        network::write_connect_request(req_writer, network::connect_request_payload{.client_nonce = 999ULL});
        req_writer.flush();
        auto client_conn = network::connection{0, server_ep};
        const auto req_bytes = client_conn.build_packet(network::packet_type::connect_request, req_writer.data(), chrono::steady_clock::now());
        const auto sent_conn = client_socket.send_to(server_ep, span<const byte>{req_bytes.data(), req_bytes.size()});
        ASSERT_GT(sent_conn, 0U);

        server.poll_network_packets();
        ASSERT_EQ(server.sessions().size(), 1U);

        // 2. Act: Send disconnect packet
        auto disc_writer = network::bit_writer{};
        network::write_disconnect(disc_writer, network::disconnect_payload{
            .reason = network::disconnect_reason::user_quit,
        });
        disc_writer.flush();
        const auto disc_packet = client_conn.build_packet(network::packet_type::disconnect, disc_writer.data(), chrono::steady_clock::now());
        const auto sent_disc = client_socket.send_to(server_ep, span<const byte>{disc_packet.data(), disc_packet.size()});
        ASSERT_GT(sent_disc, 0U);

        server.poll_network_packets();

        // 3. Assert
        EXPECT_TRUE(server.sessions().empty());
    }
} // namespace tempest::server::tests
