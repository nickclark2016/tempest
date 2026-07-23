#include <tempest/windows/viewport_window.hpp>

#include <tempest/editor_engine_context.hpp>
#include <tempest/ui.hpp>

#include <imgui.h>

namespace tempest::editor
{
    viewport_window::viewport_window(editor_engine_context& ctx) : _ctx{&ctx}, _viewport_texture{}
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

        constexpr auto window_padding_y = 2.0F;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, window_padding_y));

        if (ImGui::Begin(name.data(), &_open))
        {
            // Set up a bar to put the play/pause buttons in
            // |              | Play | Pause | Stop |          Quick Stats (Render Time, FPS, etc.) |

            const auto current_state = _ctx->get_simulation_state();

            auto draw_sim_button = [&](const char* label, simulation_state state) {
                const bool is_active = (current_state == state);
                if (is_active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    ImGui::BeginDisabled(true);
                }

                if (ImGui::Button(label))
                {
                    _ctx->set_simulation_state(state);
                }

                if (is_active)
                {
                    ImGui::EndDisabled();
                    ImGui::PopStyleColor();
                }
            };

            draw_sim_button("Play", simulation_state::play);
            ImGui::SameLine();
            draw_sim_button("Pause", simulation_state::pause);
            ImGui::SameLine();
            draw_sim_button("Stop", simulation_state::stopped);
            
            ImGui::Separator();

            const auto content_size = ImGui::GetContentRegionAvail();
            
            auto& renderer = _ctx->get_renderer();

            ImGui::BeginChild("ViewportChild", ImVec2(0, 0), 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            if ((content_size.x != static_cast<float>(_viewport_size.x) ||
                 content_size.y != static_cast<float>(_viewport_size.y)) &&
                content_size.x > 0.0F && content_size.y > 0.0F)
            {
                _viewport_size.x = static_cast<uint32_t>(content_size.x);
                _viewport_size.y = static_cast<uint32_t>(content_size.y);

                renderer.get_frame_graph().resize_render_targets(_viewport_size.x, _viewport_size.y);
                _viewport_texture = renderer.get_frame_graph().get_tonemapped_color_image();
            }

            _viewport_texture = renderer.get_frame_graph().get_tonemapped_color_image();
            ui::image(_viewport_texture, _viewport_size.x, _viewport_size.y);

            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::PopStyleVar(1);
    }

    auto viewport_window::aspect_ratio() const -> float
    {
        return static_cast<float>(_viewport_size.x) / _viewport_size.y;
    }
} // namespace tempest::editor