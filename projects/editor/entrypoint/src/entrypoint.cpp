#include <tempest/archetype.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/ui.hpp>

namespace tempest::editor
{
    static void setup_render_graph(engine_context& ctx, rhi::window_surface* win_surface, ui::ui_context* ui_ctx)
    {
        graphics::pbr_frame_graph& render_graph = ctx.get_renderer().get_frame_graph();
        auto render_graph_builder_opt = render_graph.get_builder(); // Use auto to avoid unnecessary type declaration

        if (!render_graph_builder_opt.has_value())
        {
            return;
        }

        auto& render_graph_builder = render_graph_builder_opt.value();

        auto surface = ctx.get_renderer().get_device().create_render_surface({
            .window = win_surface,
            .min_image_count = 3,
            .format =
                {
                    .space = rhi::color_space::srgb_nonlinear,
                    .format = rhi::image_format::bgra8_srgb,
                },
            .present_mode = rhi::present_mode::immediate,
            .width = win_surface->framebuffer_width(),
            .height = win_surface->framebuffer_height(),
            .layers = 1,
        });

        auto tonemapped_color_target = render_graph.get_tonemapped_color_handle();
        auto imported_surface_handle = render_graph_builder.import_render_surface("Render Surface", surface);

        auto final_color_target = ui::create_ui_pass("Editor UI Pass", *ui_ctx, render_graph_builder,
                                                     ctx.get_renderer().get_device(), tonemapped_color_target);

        render_graph_builder.create_transfer_pass(
            "Blit to Swapchain Pass",
            [&](graphics::transfer_task_builder& task) {
                task.read(final_color_target, rhi::image_layout::transfer_src,
                          make_enum_mask(rhi::pipeline_stage::blit), make_enum_mask(rhi::memory_access::transfer_read));
                task.write(imported_surface_handle, rhi::image_layout::transfer_dst,
                           make_enum_mask(rhi::pipeline_stage::blit),
                           make_enum_mask(rhi::memory_access::transfer_write));
            },
            [](graphics::transfer_task_execution_context& ctx, auto color_target, auto swapchain_handle) {
                ctx.blit(color_target, swapchain_handle);
            },
            final_color_target, imported_surface_handle);
        ;
    }

    static void run()
    {
        auto engine = tempest::engine_context{};
        auto window_data = engine.register_window(
            {
                .width = 1920,
                .height = 1080,
                .name = "Tempest Editor",
                .fullscreen = false,
            },
            false);

        auto&& [win_surface, render_surface, inputs] = window_data;
        auto ui_ctx = ui::ui_context(win_surface, &engine.get_renderer().get_device(),
                                     engine.get_renderer().get_frame_graph().get_tonemapped_color_format(), 3);

        auto camera = static_cast<ecs::archetype_entity>(ecs::null);

        engine.register_on_initialize_callback([&](engine_context& ctx) {
            setup_render_graph(ctx, win_surface, &ui_ctx);

            auto sponza_prefab = ctx.get_asset_database().import(
                "assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", ctx.get_registry());
            auto sponza_instance = ctx.load_entity(sponza_prefab);
            ctx.get_registry().get<tempest::ecs::transform_component>(sponza_instance).scale({0.125f});
            ctx.get_registry().name(sponza_instance, "Sponza");

            camera = ctx.get_registry().create();
            ctx.get_registry().name(camera, "Main Camera");

            auto camera_data = tempest::graphics::camera_component{
                .aspect_ratio = 16.0f / 9.0f,
                .vertical_fov = 100.0f,
                .near_plane = 0.01f,
                .far_shadow_plane = 64.0f,
            };

            ctx.get_registry().assign(camera, camera_data);

            auto camera_tx = tempest::ecs::transform_component::identity();
            camera_tx.position({0.0f, 15.0f, -1.0f});
            camera_tx.rotation({0.0f, tempest::math::as_radians(90.0f), 0.0f});
            ctx.get_registry().assign(camera, camera_tx);

            auto sun = ctx.get_registry().create();
            auto sun_data = tempest::graphics::directional_light_component{
                .color = {1.0f, 1.0f, 1.0f},
                .intensity = 1.0f,
            };

            auto sun_shadows = tempest::graphics::shadow_map_component{
                .size = {4096, 4096},
                .cascade_count = 4,
            };

            ctx.get_registry().assign_or_replace(sun, sun_shadows);
            ctx.get_registry().assign_or_replace(sun, sun_data);
            ctx.get_registry().name(sun, "Sun");

            auto sun_tx = tempest::ecs::transform_component::identity();
            sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});
            ctx.get_registry().assign_or_replace(sun, sun_tx);
        });

        engine.register_on_variable_update_callback(
            [&camera, &win_surface, &ui_ctx](engine_context& ctx, auto dt) mutable {
                const auto window_width = win_surface->framebuffer_width();
                const auto window_height = win_surface->framebuffer_height();
                auto& cam_data = ctx.get_registry().get<graphics::camera_component>(camera);
                cam_data.aspect_ratio = static_cast<float>(window_width) / window_height;

                ui_ctx.begin_ui_commands();

                ui_ctx.finish_ui_commands();
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
