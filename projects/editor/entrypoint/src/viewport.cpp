#include "viewport.hpp"

#include <tempest/ui.hpp>

namespace tempest::editor
{
    void viewport::render()
    {
        if (ui::ui_context::begin_window({
                .name = "Viewport",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            // Render the viewport content here
            auto win_size = ui::ui_context::get_current_window_size();
        }
        ui::ui_context::end_window();
    }

    bool viewport::should_render() const noexcept
    {
        return true;
    }

    bool viewport::should_close() const noexcept
    {
        return false;
    }

    string_view viewport::name() const noexcept
    {
        return "Viewport";
    }
}