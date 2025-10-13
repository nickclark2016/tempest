#include <tempest/frame_graph.hpp>
#include <tempest/input.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/pipelines/pbr_pipeline.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>

namespace tempest::rhi::vk
{
    unique_ptr<rhi::instance> create_instance() noexcept;
    unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

int main()
{
    // auto engine = tempest::engine_context();
    // auto surface_desc = tempest::rhi::window_surface_desc{
    //     .width = 1920,
    //     .height = 1080,
    //     .name = "Tempest Engine Sandbox",
    //     .fullscreen = false,
    // };

    // auto window_and_inputs = engine.register_window(surface_desc);
    // auto& [surface, inputs] = window_and_inputs;
    // auto pipeline =
    //     engine.register_pipeline<tempest::graphics::pbr_pipeline>(surface, 1920, 1080, engine.get_registry());

    // engine.register_on_initialize_callback([&](tempest::engine_context& ctx) {
    //     auto sponza_prefab =
    //     ctx.get_asset_database().import("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf",
    //                                                          ctx.get_registry());
    //     auto sponza_instance = ctx.load_entity(sponza_prefab);
    //     ctx.get_registry().get<tempest::ecs::transform_component>(sponza_instance).scale({0.125f});

    //    auto skybox_texture_prefab =
    //        ctx.get_asset_database().import("assets/polyhaven/hdri/autumn_field_puresky.exr", ctx.get_registry());
    //    auto skybox_texture =
    //        ctx.get_registry().get<tempest::core::texture_component>(skybox_texture_prefab).texture_id;
    //    pipeline->set_skybox_texture(ctx.get_renderer().get_device(), skybox_texture, ctx.get_texture_registry());

    //    auto camera = ctx.get_registry().create();
    //    tempest::graphics::camera_component camera_data = {
    //        .aspect_ratio = 16.0f / 9.0f,
    //        .vertical_fov = 100.0f,
    //        .near_plane = 0.01f,
    //        .far_shadow_plane = 64.0f,
    //    };

    //    ctx.get_registry().assign(camera, camera_data);
    //    auto camera_tx = tempest::ecs::transform_component::identity();
    //    camera_tx.position({0.0f, 15.0f, -1.0f});
    //    camera_tx.rotation({0.0f, tempest::math::as_radians(90.0f), 0.0f});
    //    ctx.get_registry().assign(camera, camera_tx);

    //    auto sun = ctx.get_registry().create();
    //    tempest::graphics::directional_light_component sun_data = {
    //        .color = {1.0f, 1.0f, 1.0f},
    //        .intensity = 1.0f,
    //    };

    //    tempest::graphics::shadow_map_component sun_shadows = {
    //        .size = {2048, 2048},
    //        .cascade_count = 3,
    //    };

    //    surface->register_resize_callback([&ctx, camera](uint32_t width, uint32_t height) {
    //        auto camera_data = ctx.get_registry().get<tempest::graphics::camera_component>(camera);
    //        camera_data.aspect_ratio = static_cast<float>(width) / height;
    //        ctx.get_registry().assign_or_replace(camera, camera_data);
    //    });

    //    ctx.get_registry().assign_or_replace(sun, sun_shadows);
    //    ctx.get_registry().assign_or_replace(sun, sun_data);
    //    ctx.get_registry().name(sun, "Sun");

    //    tempest::ecs::transform_component sun_tx = tempest::ecs::transform_component::identity();
    //    sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});

    //    ctx.get_registry().assign_or_replace(sun, sun_tx);
    //});

    // engine.run();

    auto instance = tempest::rhi::vk::create_instance();
    auto& device = instance->acquire_device(0);

    auto window = tempest::rhi::vk::create_window_surface({
        .width = 1280,
        .height = 720,
        .name = "Tempest Render Graph Test",
        .fullscreen = false,
    });

    auto swapchain = device.create_render_surface({
        .window = window.get(),
        .min_image_count = 2,
        .format =
            {
                .space = tempest::rhi::color_space::srgb_nonlinear,
                .format = tempest::rhi::image_format::bgra8_srgb,
            },
        .present_mode = tempest::rhi::present_mode::immediate,
        .width = 1280,
        .height = 720,
        .layers = 1,
    });

    auto render_graph_builder = tempest::graphics::graph_builder{};
    auto swapchain_handle = render_graph_builder.import_render_surface("Swapchain", swapchain);

    render_graph_builder.create_transfer_pass(
        "Write to Swapchain",
        [&](tempest::graphics::transfer_task_builder& builder) {
            builder.write(swapchain_handle, tempest::rhi::image_layout::transfer_dst,
                          tempest::make_enum_mask(tempest::rhi::pipeline_stage::clear),
                          tempest::make_enum_mask(tempest::rhi::memory_access::transfer_write));
        },
        [](tempest::graphics::transfer_task_execution_context& ctx, auto swapchain_handle) {
            ctx.clear_color(swapchain_handle, 1.0f, 0.0f, 0.0f, 1.0f);
        },
        swapchain_handle);

    auto plan = tempest::move(render_graph_builder)
                    .compile({
                        .graphics_queues = 1,
                        .compute_queues = 1,
                        .transfer_queues = 1,
                    });

    auto executor = tempest::graphics::graph_executor(device);
    executor.set_execution_plan(tempest::move(plan));

    auto pbr_fg =
        tempest::graphics::pbr_frame_graph(device,
                                           {
                                               .render_target_width = 1280,
                                               .render_target_height = 720,
                                               .shadow_map_width = 2048,
                                               .shadow_map_height = 2048,
                                               .hdr_color_format = tempest::rhi::image_format::rgba16_float,
                                               .depth_format = tempest::rhi::image_format::d32_float,
                                               .tonemapped_color_format = tempest::rhi::image_format::bgra8_srgb,
                                               .vertex_data_buffer_size = 16 * 1024 * 1024,
                                               .mesh_data_buffer_size = 16 * 1024 * 1024,
                                               .material_data_buffer_size = 4 * 1024 * 1024,
                                               .staging_buffer_size_per_frame = 16 * 1024 * 1024,
                                               .max_lights = 256,
                                               .max_anisotropy = 16.0f,
                                           },
                                           {
                                               .entity_registry = nullptr,
                                           });

    pbr_fg.get_builder()->import_render_surface("Swapchain", swapchain);

    pbr_fg.compile({
        .graphics_queues = 1,
        .compute_queues = 1,
        .transfer_queues = 1,
    });

    while (true)
    {
        tempest::core::input::poll();

        if (window->should_close())
        {
            break;
        }

        executor.execute();
    }

    return 0;
}