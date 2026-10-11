#ifndef tempest_editor_core_log_view_window_hpp
#define tempest_editor_core_log_view_window_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/log_store.hpp>
#include <tempest/string_view.hpp>
#include <tempest/windows/editor_window.hpp>

namespace tempest::editor
{
    class TEMPEST_EDITOR_API log_view_window final : public editor_window
    {
      public:
        static constexpr bool default_open = true;
        static constexpr bool default_auto_scroll = true;
        static constexpr bool default_request_scroll = false;
        static constexpr size_t search_buffer_capacity = 256;

        explicit log_view_window(log_store& store);
        ~log_view_window() override = default;

        log_view_window(const log_view_window&) = delete;
        log_view_window(log_view_window&&) noexcept = delete;
        auto operator=(const log_view_window&) -> log_view_window& = delete;
        auto operator=(log_view_window&&) noexcept -> log_view_window& = delete;

        [[nodiscard]] auto desired_initial_dock() const -> dock_location override
        {
            return dock_location::bottom;
        }

        [[nodiscard]] auto window_name() const -> string_view override
        {
            return "Log View";
        }

        [[nodiscard]] auto is_mode_supported(simulation_state) const noexcept -> bool override
        {
            return true;
        }

        auto draw() -> void override;

        log_store* store = nullptr;
        bool open = default_open;
        bool auto_scroll = default_auto_scroll;
        bool request_scroll_to_bottom = default_request_scroll;
        char search_buffer[search_buffer_capacity] = "";
    };
} // namespace tempest::editor

#endif // tempest_editor_core_log_view_window_hpp
