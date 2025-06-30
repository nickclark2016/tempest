#include <tempest/tempest.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    struct window_context
    {
        unique_ptr<rhi::window_surface> surface;
        unique_ptr<ui::ui_context> ui_context;
    };

    struct editor_context
    {
        vector<window_context> windows;
        unique_ptr<graphics::renderer> renderer;
    };

    void run()
    {
        auto engine = tempest::engine_context{};
        auto [win_surface, inputs] = engine.register_window({
            .width = 1920,
            .height = 1080,
            .name = "Tempest Editor",
            .fullscreen = false,
        });

        auto ui_context = tempest::make_unique<ui::ui_context>(win_surface, &engine.get_renderer().get_device(),
                                                               rhi::image_format::bgra8_srgb, 3);
        auto ui_pipeline = engine.register_pipeline<ui::ui_pipeline>(win_surface, ui_context.get());
        ui_pipeline->set_size(win_surface->framebuffer_width(), win_surface->framebuffer_height());

        engine.register_on_fixed_update_callback([&win_surface, &ui_context, dockspace_needs_configure = true,
                                                  dockspace_layout = ui::ui_context::dockspace_configure_info{},
                                                  dockspace_info = ui::ui_context::dockspace_info{},
                                                  dockspace_ids = ui::ui_context::dockspace_identifiers{}](
                                                     engine_context& ctx, auto dt) mutable {
            ui_context->begin_ui_commands();

            if (ui::ui_context::begin_window({
                    .name = "Tempest Editor",
                    .position = none(),
                    .size = none(),
                    .flags = make_enum_mask(
                        ui::ui_context::window_flags::no_title, ui::ui_context::window_flags::no_resize,
                        ui::ui_context::window_flags::no_move, ui::ui_context::window_flags::no_collapse,
                        ui::ui_context::window_flags::no_bring_to_front_on_focus,
                        ui::ui_context::window_flags::no_navigation_focus, ui::ui_context::window_flags::no_decoration,
                        ui::ui_context::window_flags::no_docking),
                }))
            {
                if (dockspace_needs_configure)
                {
                    dockspace_layout.name = "MainDockspace";
                    dockspace_layout.left_window_name = "Entities";
                    dockspace_layout.left_width = 0.2f;
                    dockspace_layout.right_width = 0.2f;

                    dockspace_needs_configure = false;

                    dockspace_ids = ui::ui_context::configure_dockspace(dockspace_layout);
                    dockspace_info.root = dockspace_ids.root_id;
                }

                ui::ui_context::dockspace(dockspace_info);
            }
            ui::ui_context::end_window();

            ui::ui_context::begin_window({
                .name = "Entities",
                .position = none(),
                .size = math::vec2(240.0f, 100.0f),
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            });
            ui::ui_context::end_window();

            ui_context->finish_ui_commands();
        });

        engine.run();
    }
} // namespace tempest::editor

#if _WIN32

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    tempest::editor::run();
    return 0;
}

#else

int main(int argc, char* argv[])
{
    tempest::editor::run();
    return 0;
}

#endif
