#include "viewport.hpp"

#include <tempest/ui.hpp>

namespace tempest::editor
{
    void viewport::render()
    {
        ui::ui_context::push_window_padding(0.0f, 0.0f);

        if (ui::ui_context::begin_window({
                .name = "Viewport",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            // Render the viewport content here
            _win_size = ui::ui_context::get_available_content_region();
            _visible = true;

            if (_pipeline)
            {
                auto render_target = _pipeline->get_render_target();
                if (render_target.image && render_target.layout == rhi::image_layout::shader_read_only)
                {
                    ui::ui_context::image(render_target.image, _win_size.x, _win_size.y);
                }
            }
        }
        else
        {
            _visible = false;
        }

        ui::ui_context::end_window();

        ui::ui_context::pop_window_padding();
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