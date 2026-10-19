#include <tempest/windows/viewport_window.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/editor_engine_context.hpp>
#include <tempest/play_mode_controller.hpp>
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
            auto& controller = _ctx->get_play_mode_controller();
            const auto current_state = controller.get_state();

            const auto is_playing = (current_state == simulation_state::playing);
            if (is_playing)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Play"))
            {
                controller.play();
            }
            if (is_playing)
            {
                ImGui::EndDisabled();
                ImGui::PopStyleColor();
            }

            ImGui::SameLine();

            const auto is_paused = (current_state == simulation_state::paused);
            const auto can_pause = is_playing;
            if (!can_pause && !is_paused)
            {
                ImGui::BeginDisabled(true);
            }
            if (is_paused)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Pause"))
            {
                controller.pause();
            }
            if (is_paused)
            {
                ImGui::EndDisabled();
                ImGui::PopStyleColor();
            }
            else if (!can_pause)
            {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            const auto can_step = is_paused;
            if (!can_step)
            {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Step"))
            {
                controller.step();
            }
            if (!can_step)
            {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();

            const auto can_stop = (current_state != simulation_state::edit);
            if (!can_stop)
            {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("Stop"))
            {
                controller.stop();
            }
            if (!can_stop)
            {
                ImGui::EndDisabled();
            }

            if (_ctx->get_config().headless)
            {
                ImGui::BeginChild("ViewportChild", ImVec2(0, 0), 0,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::TextUnformatted("Headless Viewport (No Renderer)");
                ImGui::EndChild();
            }
            else
            {
                auto& renderer = _ctx->get_renderer();

                ImGui::BeginChild("ViewportChild", ImVec2(0, 0), 0,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                const auto content_size = ImGui::GetContentRegionAvail();
                const auto new_width = static_cast<uint32_t>(tempest::max(0.0F, content_size.x));
                const auto new_height = static_cast<uint32_t>(tempest::max(0.0F, content_size.y));

                if ((new_width != _viewport_size.x || new_height != _viewport_size.y) && new_width > 0 &&
                    new_height > 0)
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

                const auto is_hovered =
                    ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) || ImGui::IsItemHovered();
                const auto is_active = ImGui::IsItemActive();
                auto& io = ImGui::GetIO();
                auto& cam = _ctx->get_editor_camera();

                // Left click in viewport while in play mode to capture cursor
                if (current_state == simulation_state::playing && (is_hovered || is_active) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    const auto main_win = _ctx->get_main_window();
                    if (main_win.is_valid())
                    {
                        _ctx->get_window_manager().set_cursor_mode(main_win, cursor_mode::disabled);
                    }
                }

                // Right click hold for fly camera navigation (active in edit/pause modes)
                if (current_state != simulation_state::playing && (is_hovered || is_active) &&
                    ImGui::IsMouseDown(ImGuiMouseButton_Right))
                {
                    // Mouse look
                    const auto mouse_delta = io.MouseDelta;
                    if (mouse_delta.x != 0.0F || mouse_delta.y != 0.0F)
                    {
                        constexpr auto sensitivity = 0.003F;
                        cam.rotate(mouse_delta.x * sensitivity, mouse_delta.y * sensitivity);
                    }

                    // Speed modifier
                    auto speed = cam.get_move_speed();
                    if (io.KeyShift)
                    {
                        speed *= 2.5F;
                    }

                    const auto dt = io.DeltaTime > 0.0F ? io.DeltaTime : (1.0F / 60.0F);

                    // WASD / QE navigation
                    auto forward = 0.0F;
                    auto right = 0.0F;
                    auto up = 0.0F;

                    if (ImGui::IsKeyDown(ImGuiKey_W))
                    {
                        forward += speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_S))
                    {
                        forward -= speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_D))
                    {
                        right += speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_A))
                    {
                        right -= speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_E))
                    {
                        up += speed * dt;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_Q))
                    {
                        up -= speed * dt;
                    }

                    if (forward != 0.0F || right != 0.0F || up != 0.0F)
                    {
                        cam.move(forward, right, up);
                    }

                    // Mouse wheel speed adjustment
                    if (io.MouseWheel != 0.0F)
                    {
                        cam.adjust_move_speed(io.MouseWheel * 0.5F);
                    }
                }

                ImGui::EndChild();
            }
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