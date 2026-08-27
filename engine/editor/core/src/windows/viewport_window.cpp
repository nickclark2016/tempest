#include <tempest/windows/viewport_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/ui.hpp>

#include <imgui.h>

namespace tempest::editor
{
    viewport_window::viewport_window(editor_engine_context& ctx) : _ctx{&ctx}, _viewport_size{0, 0}
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
            const auto current_state = _ctx->get_simulation_state();

            auto draw_sim_button = [&](const char* label, simulation_state state) {
                const auto is_active = (current_state == state);
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

            auto& renderer = _ctx->get_renderer();

            ImGui::BeginChild("ViewportChild", ImVec2(0, 0), 0,
                              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            const auto content_size = ImGui::GetContentRegionAvail();
            const auto new_width = static_cast<uint32_t>(tempest::max(0.0F, content_size.x));
            const auto new_height = static_cast<uint32_t>(tempest::max(0.0F, content_size.y));

            if ((new_width != _viewport_size.x || new_height != _viewport_size.y) && new_width > 0 && new_height > 0)
            {
                _viewport_size.x = new_width;
                _viewport_size.y = new_height;
                renderer.resize(new_width, new_height);
            }

            auto rg_tex_id = renderer.get_tonemapped_color_texture();
            const auto* alloc = renderer.get_render_graph().get_allocator().get_texture(rg_tex_id.id);
            if (alloc && alloc->sampled_descriptor.index != ~0U && _viewport_size.x > 0 && _viewport_size.y > 0)
            {
                ui::image(alloc->sampled_descriptor, _viewport_size.x, _viewport_size.y);
            }

            ImGui::EndChild();
        }

        ImGui::End();

        ImGui::PopStyleVar(1);
    }

    auto viewport_window::aspect_ratio() const -> float
    {
        return _viewport_size.y > 0 ? static_cast<float>(_viewport_size.x) / static_cast<float>(_viewport_size.y)
                                    : 1.0F;
    }
} // namespace tempest::editor