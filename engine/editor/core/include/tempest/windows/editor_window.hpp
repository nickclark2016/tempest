#ifndef tempest_editor_core_editor_window_hpp
#define tempest_editor_core_editor_window_hpp

#include <tempest/int.hpp>
#include <tempest/string_view.hpp>

#include <tempest/editor_engine_context.hpp>

namespace tempest::editor
{
    /// @brief Base class for all editor windows.
    class TEMPEST_EDITOR_API editor_window
    {
      public:
        /// @brief Enumeration of possible dock locations for the editor window.
        enum class dock_location : uint8_t
        {
            /// @brief No specific dock location.
            none,
            /// @brief Docked in the center of the editor.
            center,
            /// @brief Docked to the left side of the editor.
            left,
            /// @brief Docked to the right side of the editor.
            right,
            /// @brief Docked to the bottom of the editor.
            bottom,
        };

        editor_window(const editor_window&) = delete;
        editor_window(editor_window&&) noexcept = delete;
        virtual ~editor_window() = default;

        auto operator=(const editor_window&) -> editor_window& = delete;
        auto operator=(editor_window&&) noexcept -> editor_window& = delete;

        /// @brief Obtains the desired initial dock location for the editor window.
        /// @return The preferred dock location.
        [[nodiscard]] virtual auto desired_initial_dock() const -> dock_location = 0;

        /// @brief Obtains the display name of the editor window.
        /// @return The window name as a string view.
        [[nodiscard]] virtual auto window_name() const -> string_view = 0;

        /// @brief Renders the contents of the editor window.
        virtual auto draw() -> void = 0;

        /// @brief Determines if the editor window supports a specific simulation state.
        /// @param state The simulation state to check.
        /// @return True if the state is supported, false otherwise.
        [[nodiscard]] virtual auto is_mode_supported(simulation_state state) const noexcept -> bool
        {
            return state != simulation_state::playing;
        }

      protected:
        editor_window() = default;
    };
} // namespace tempest::editor

#endif // tempest_editor_core_editor_window_hpp