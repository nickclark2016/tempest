#ifndef tempest_network_prediction_buffer_hpp
#define tempest_network_prediction_buffer_hpp

#include <tempest/array.hpp>
#include <tempest/int.hpp>
#include <tempest/network/user_cmd.hpp>
#include <tempest/optional.hpp>
#include <tempest/span.hpp>

namespace tempest::network
{
    template <typename StateSnapshot>
    struct predicted_frame
    {
        user_cmd cmd = {};
        StateSnapshot predicted_state = {};
        bool is_valid = false;
    };

    template <typename StateSnapshot, size_t Capacity = 128>
    class client_prediction_buffer
    {
      public:
        client_prediction_buffer() = default;

        /// @brief Records an input command and resulting predicted state for the specified tick.
        /// If a frame already exists at this circular buffer slot (e.g. from an older tick),
        /// it is overwritten with the new tick's data.
        /// @param cmd The user input command for the tick.
        /// @param state The simulated/predicted state snapshot.
        /// @return Reference to the newly stored predicted frame.
        auto record_input(const user_cmd& cmd, const StateSnapshot& state) -> predicted_frame<StateSnapshot>&
        {
            const auto slot_index = cmd.tick % Capacity;
            _frames[slot_index] = predicted_frame<StateSnapshot>{
                .cmd = cmd,
                .predicted_state = state,
                .is_valid = true,
            };

            if (!_latest_tick.has_value() || cmd.tick > *_latest_tick)
            {
                _latest_tick = cmd.tick;
            }

            return _frames[slot_index];
        }

        /// @brief Retrieves the predicted frame for the given tick if valid and not overwritten.
        /// @param tick Simulation tick number.
        /// @return Pointer to the frame if found and valid, or nullptr.
        [[nodiscard]] auto get_frame(uint32_t tick) noexcept -> predicted_frame<StateSnapshot>*
        {
            auto& frame = _frames[tick % Capacity];
            if (frame.is_valid && frame.cmd.tick == tick)
            {
                return &frame;
            }
            return nullptr;
        }

        /// @brief Retrieves the predicted frame for the given tick if valid and not overwritten.
        /// @param tick Simulation tick number.
        /// @return Const pointer to the frame if found and valid, or nullptr.
        [[nodiscard]] auto get_frame(uint32_t tick) const noexcept -> const predicted_frame<StateSnapshot>*
        {
            const auto& frame = _frames[tick % Capacity];
            if (frame.is_valid && frame.cmd.tick == tick)
            {
                return &frame;
            }
            return nullptr;
        }

        /// @brief Checks whether a valid frame exists for the specified tick.
        [[nodiscard]] auto has_frame(uint32_t tick) const noexcept -> bool
        {
            return get_frame(tick) != nullptr;
        }

        /// @brief Discards all inputs and predicted frames acknowledged by the server up to and
        /// including last_acked_server_tick.
        /// @param last_acked_server_tick The highest tick acknowledged by the server.
        auto discard_acked_inputs(uint32_t last_acked_server_tick) noexcept -> void
        {
            if (!_last_acked_tick.has_value() || last_acked_server_tick > *_last_acked_tick)
            {
                _last_acked_tick = last_acked_server_tick;
            }

            for (auto& frame : _frames)
            {
                if (frame.is_valid && frame.cmd.tick <= last_acked_server_tick)
                {
                    frame.is_valid = false;
                }
            }
        }

        /// @brief Gathers unacknowledged commands from the buffer up to out_cmds.size().
        /// Specifically, walks backwards from latest_tick down to the first unacknowledged tick
        /// (where tick > last_acked_tick if last_acked_tick has a value), and writes the gathered
        /// commands into out_cmds in chronological order (oldest to newest).
        /// @param out_cmds Destination span to receive the commands.
        /// @return Number of commands written to out_cmds.
        auto gather_unacked_commands(span<user_cmd> out_cmds) const -> size_t
        {
            if (!_latest_tick.has_value() || out_cmds.empty())
            {
                return 0;
            }

            const auto latest = *_latest_tick;
            if (_last_acked_tick.has_value() && latest <= *_last_acked_tick)
            {
                return 0;
            }

            const auto max_gather = out_cmds.size();
            auto count = static_cast<size_t>(0);

            // Traverse from latest tick backwards to collect unacknowledged ticks
            auto current_tick = latest;
            while (count < max_gather)
            {
                if (_last_acked_tick.has_value() && current_tick <= *_last_acked_tick)
                {
                    break;
                }

                const auto* frame = get_frame(current_tick);
                if (frame == nullptr || !frame->is_valid)
                {
                    break;
                }

                ++count;

                if (current_tick == 0)
                {
                    break;
                }
                --current_tick;
            }

            // Write into out_cmds in chronological order (oldest to newest)
            for (size_t i = 0; i < count; ++i)
            {
                const auto tick_to_fetch = latest - (count - 1U - i);
                const auto* frame = get_frame(tick_to_fetch);
                if (frame != nullptr)
                {
                    out_cmds[i] = frame->cmd;
                }
            }

            return count;
        }

        /// @brief Returns the latest tick recorded in the buffer, or nullopt if empty.
        [[nodiscard]] auto latest_tick() const noexcept -> optional<uint32_t>
        {
            return _latest_tick;
        }

        /// @brief Returns the highest tick acknowledged by the server, or nullopt if none.
        [[nodiscard]] auto last_acked_tick() const noexcept -> optional<uint32_t>
        {
            return _last_acked_tick;
        }

        /// @brief Resets the prediction buffer to its empty initial state.
        auto reset() noexcept -> void
        {
            for (auto& frame : _frames)
            {
                frame = predicted_frame<StateSnapshot>{};
            }
            _latest_tick = nullopt;
            _last_acked_tick = nullopt;
        }

        /// @brief Returns the fixed capacity of the circular buffer.
        [[nodiscard]] static constexpr auto capacity() noexcept -> size_t
        {
            return Capacity;
        }

      private:
        array<predicted_frame<StateSnapshot>, Capacity> _frames = {};
        optional<uint32_t> _latest_tick = nullopt;
        optional<uint32_t> _last_acked_tick = nullopt;
    };
} // namespace tempest::network

#endif // tempest_network_prediction_buffer_hpp
