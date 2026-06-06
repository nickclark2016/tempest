#include <tempest/windows/viewport_window.hpp>

#include <tempest/ui.hpp>

#include <imgui.h>

namespace tempest::editor
{
    viewport_window::viewport_window(graphics::renderer& renderer) : _renderer{&renderer}, _viewport_texture{}
    {
    }

    auto viewport_window::desired_initial_dock() const -> editor_window::dock_location
    {
        return dock_location::center;
    }

    auto viewport_window::window_name() const -> string_view
    {
        return "Viewport";
    }

    auto viewport_window::draw() -> void
    {
        const auto name = window_name();
        if (ImGui::Begin(name.data(), &_open))
        {
            const auto content_size = ImGui::GetContentRegionAvail();

            if ((content_size.x != _viewport_size.x || content_size.y != _viewport_size.y) && content_size.x != 0 && content_size.y != 0)
            {
                _viewport_size.x = static_cast<uint32_t>(content_size.x);
                _viewport_size.y = static_cast<uint32_t>(content_size.y);

                _renderer->get_frame_graph().resize_render_targets(_viewport_size.x, _viewport_size.y);
                _viewport_texture = _renderer->get_frame_graph().get_tonemapped_color_image();
            }

            _viewport_texture = _renderer->get_frame_graph().get_tonemapped_color_image();
            ui::image(_viewport_texture, _viewport_size.x, _viewport_size.y);
        }

        ImGui::End();
    }

    auto viewport_window::aspect_ratio() const -> float
    {
        return static_cast<float>(_viewport_size.x) / _viewport_size.y;
    }
} // namespace tempest::editor