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


    void draw_entity_hierarchy_window()
    {
        if (ui::ui_context::begin_window({
                .name = "Scene",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
        }

        ui::ui_context::end_window();
    }

    void draw_entity_inspector_window()
    {
        if (ui::ui_context::begin_window({
                .name = "Entity Inspector",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
        }

        ui::ui_context::end_window();
    }

    void draw_assets_window()
    {
        if (ui::ui_context::begin_window({
                .name = "Assets",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
        }
        ui::ui_context::end_window();
    }

    void draw_viewport_window()
    {
        if (ui::ui_context::begin_window({
                .name = "Viewport",
                .position = ui::ui_context::default_position_tag,
                .size = ui::ui_context::default_size_tag,
                .flags = make_enum_mask(ui::ui_context::window_flags::none),
            }))
        {
            // Placeholder for viewport rendering
        }
        ui::ui_context::end_window();
    }

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
        ui_pipeline->set_viewport(win_surface->framebuffer_width(), win_surface->framebuffer_height());

        engine.register_on_fixed_update_callback([&win_surface, &ui_context, dockspace_needs_configure = true,
                                                  dock_layout = ui::ui_context::dockspace_layout{}](engine_context& ctx,
                                                                                                    auto dt) mutable {
            ui_context->begin_ui_commands();

            if (ui::ui_context::begin_window({
                    .name = "Editor Dockspace",
                    .position = ui::ui_context::viewport_origin_tag,
                    .size = ui::ui_context::fullscreen_tag,
                    .flags = make_enum_mask(
                        ui::ui_context::window_flags::no_title, ui::ui_context::window_flags::no_collapse,
                        ui::ui_context::window_flags::no_resize, ui::ui_context::window_flags::no_move,
                        ui::ui_context::window_flags::no_bring_to_front_on_focus,
                        ui::ui_context::window_flags::no_navigation_focus, ui::ui_context::window_flags::no_docking),
                }))
            {
                ui::ui_context::dockspace(ui::ui_context::get_dockspace_id("TempestDockspace"));

                if (dockspace_needs_configure)
                {
                    using ds_config_node = ui::ui_context::dockspace_configure_node;

                    auto dock_configure = ds_config_node{};

                    dock_configure.docked_windows.push_back("Viewport");
                    dock_configure.left = make_unique<ds_config_node>();
                    dock_configure.left->docked_windows.push_back("Scene");
                    dock_configure.left->size = 0.2f;
                    dock_configure.right = make_unique<ds_config_node>();
                    dock_configure.right->docked_windows.push_back("Entity Inspector");
                    dock_configure.right->size = 0.2f;
                    dock_configure.bottom = make_unique<ds_config_node>();
                    dock_configure.bottom->docked_windows.push_back("Assets");
                    dock_configure.bottom->size = 0.2f;

                    auto dock_config_info = ui::ui_context::dockspace_configure_info{
                        .root = tempest::move(dock_configure),
                        .name = "TempestDockspace",
                    };

                    dock_layout = ui_context->configure_dockspace(tempest::move(dock_config_info));

                    dockspace_needs_configure = false;
                }
            }

            ui::ui_context::end_window();

            draw_assets_window();
            draw_entity_hierarchy_window();
            draw_entity_inspector_window();
            draw_viewport_window();

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
