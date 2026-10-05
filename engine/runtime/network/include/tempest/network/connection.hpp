#ifndef tempest_network_connection_hpp
#define tempest_network_connection_hpp

#include <tempest/api.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/network/ack_tracker.hpp>
#include <tempest/network/endpoint.hpp>
#include <tempest/network/packet.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::network
{
    /// @brief Connection lifecycle state.
    enum class connection_state : uint8_t
    {
        disconnected,
        connecting,
        connected,
        disconnecting,
    };

    /// @brief Inbound packet view after wire validation and optional decryption.
    struct incoming_packet
    {
        packet_header header;
        span<const byte> payload;
    };

    /// @brief Security filter hook for future AEAD (ChaCha20-Poly1305) payload encryption.
    struct TEMPEST_API ipacket_security
    {
        ipacket_security(const ipacket_security&) = delete;
        ipacket_security(ipacket_security&&) = delete;
        virtual ~ipacket_security() = default;
        
        auto operator=(const ipacket_security&) -> ipacket_security& = delete;
        auto operator=(ipacket_security&&) -> ipacket_security& = delete;

        /// @brief Encrypts plaintext payload into out_ciphertext.
        /// @return true if encryption succeeded, false otherwise.
        virtual auto encrypt_payload(uint64_t session_id, uint32_t sequence_id, span<const byte> plaintext,
                                     vector<byte>& out_ciphertext) -> bool = 0;

        /// @brief Decrypts ciphertext into out_plaintext.
        /// @return true if decryption and authentication succeeded, false otherwise.
        virtual auto decrypt_payload(uint64_t session_id, uint32_t sequence_id, span<const byte> ciphertext,
                                     vector<byte>& out_plaintext) -> bool = 0;

      protected:
        ipacket_security() = default;
    };

    /// @brief Manages a logical session with a remote endpoint.
    /// Handles packet framing, sequence allocation, ACK tracking, and optional encryption.
    class TEMPEST_API connection
    {
      public:
        connection(uint64_t session_id, const endpoint& remote_endpoint, ipacket_security* security = nullptr);

        [[nodiscard]] auto session_id() const noexcept -> uint64_t
        {
            return _session_id;
        }
        
        auto set_session_id(uint64_t session_id) noexcept -> void
        {
            _session_id = session_id;
        }

        [[nodiscard]] auto remote_endpoint() const noexcept -> const endpoint&
        {
            return _remote_endpoint;
        }
        
        auto set_remote_endpoint(const endpoint& endpoint_target) noexcept -> void
        {
            _remote_endpoint = endpoint_target;
        }

        [[nodiscard]] auto state() const noexcept -> connection_state
        {
            return _state;
        }

        auto set_state(connection_state new_state) noexcept -> void
        {
            _state = new_state;
        }

        /// @brief Frames an outgoing packet with the 27-byte header, invokes security filter if present,
        /// and records the sequence number in the ACK tracker.
        [[nodiscard]] auto build_packet(packet_type type, span<const byte> payload,
                                        chrono::steady_clock::time_point now) -> vector<byte>;

        /// @brief Zero-allocation packet framing writing directly into out_buffer.
        /// @return Number of bytes written, or 0 if out_buffer is too small.
        auto build_packet(packet_type type, span<const byte> payload, span<byte> out_buffer,
                          chrono::steady_clock::time_point now) -> size_t;

        /// @brief Validates an incoming wire packet, updates the ACK tracker, and optionally decrypts the payload.
        [[nodiscard]] auto process_packet(span<const byte> packet_data, chrono::steady_clock::time_point now)
            -> optional<incoming_packet>;

        /// @brief Returns smoothed roundtrip time in milliseconds.
        [[nodiscard]] auto smoothed_rtt_ms() const noexcept -> float
        {
            return _ack_tracker.smoothed_rtt_ms();
        }

        /// @brief Returns packet loss rate estimate in range [0.0, 1.0].
        [[nodiscard]] auto packet_loss_rate() const noexcept -> float
        {
            return _ack_tracker.packet_loss_rate();
        }

        /// @brief Const reference to the underlying ACK tracker.
        [[nodiscard]] auto tracker() const noexcept -> const ack_tracker&
        {
            return _ack_tracker;
        }

        /// @brief Mutable reference to the underlying ACK tracker.
        [[nodiscard]] auto tracker() noexcept -> ack_tracker&
        {
            return _ack_tracker;
        }

      private:
        uint64_t _session_id = 0;
        endpoint _remote_endpoint;
        connection_state _state = connection_state::disconnected;
        ack_tracker _ack_tracker;
        ipacket_security* _security = nullptr;
        vector<byte> _send_scratch;
        vector<byte> _encryption_scratch;
        vector<byte> _decryption_scratch;
    };
} // namespace tempest::network

#endif // tempest_network_connection_hpp
