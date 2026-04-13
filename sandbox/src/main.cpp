#include <tempest/asset_database.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/frame_graph.hpp>
#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/tempest.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

namespace tempest::rhi::vk
{
    unique_ptr<rhi::instance> create_instance(tempest::logger* log, bool headless) noexcept;
    unique_ptr<rhi::window_surface> create_window_surface(const rhi::window_surface_desc& desc) noexcept;
} // namespace tempest::rhi::vk

auto main() -> int
{
    auto stdout_sink = tempest::stdout_log_sink();
    auto logger = tempest::logger(stdout_sink);

    auto instance = tempest::rhi::vk::create_instance(&logger);
    auto& device = instance->acquire_device(0);

    const auto width = 1920;
    const auto height = 1080;

    auto window = tempest::rhi::vk::create_window_surface({
        .width = width,
        .height = height,
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
        .width = width,
        .height = height,
        .layers = 1,
    });

    auto event_registry = tempest::event::event_registry();
    auto entity_registry = tempest::ecs::archetype_registry(event_registry);
    auto mesh_registry = tempest::core::mesh_registry();
    auto texture_registry = tempest::core::texture_registry();
    auto material_registry = tempest::core::material_registry();
    auto asset_type_reg = tempest::assets::asset_type_registry();
    auto asset_database = tempest::assets::asset_database(&asset_type_reg);
    tempest::assets::register_default_importers(asset_database, &mesh_registry, &texture_registry, &material_registry);
    asset_database.open("sandbox.tassetdb");

    const auto sponza_prefab =
        asset_database.load("assets/glTF-Sample-Assets/Models/Sponza/glTF/Sponza.gltf", entity_registry);

    const auto sponza_instance = entity_registry.duplicate(sponza_prefab);
    auto sponza_transform = tempest::ecs::transform_component{};
    sponza_transform.scale({0.125F});
    entity_registry.assign_or_replace(sponza_instance, sponza_transform);

    auto entities = tempest::vector<tempest::ecs::archetype_entity>{};
    entities.push_back(sponza_instance);

    auto pbr_fg =
        tempest::graphics::pbr_frame_graph(device,
                                           {
                                               .render_target_width = 1920,
                                               .render_target_height = 1080,
                                               .hdr_color_format = tempest::rhi::image_format::rgba16_float,
                                               .depth_format = tempest::rhi::image_format::d32_float,
                                               .tonemapped_color_format = tempest::rhi::image_format::bgra8_srgb,
                                               .vertex_data_buffer_size = 16 * 1024 * 1024,
                                               .max_mesh_count = 16 * 1024 * 1024,
                                               .max_material_count = 4 * 1024 * 1024,
                                               .staging_buffer_size_per_frame = 16 * 1024 * 1024,
                                               .max_object_count = 256 * 1024,
                                               .max_lights = 256,
                                               .max_bindless_textures = 1024,
                                               .max_anisotropy = 16.0f,
                                               .light_clustering =
                                                   {
                                                       .cluster_count_x = 16,
                                                       .cluster_count_y = 9,
                                                       .cluster_count_z = 24,
                                                       .max_lights_per_cluster = 128,
                                                   },
                                               .shadows =
                                                   {
                                                       .directional_shadow_map_width = 16384,
                                                       .directional_shadow_map_height = 8192,
                                                       .max_shadow_casting_lights = 16,
                                                   },
                                           },
                                           {
                                               .entity_registry = &entity_registry,
                                           });

    auto render_graph_builder_opt = pbr_fg.get_builder();
    auto& render_graph_builder = render_graph_builder_opt.value();
    auto swapchain_handle = render_graph_builder.import_render_surface("Swapchain", swapchain);
    auto color_target = pbr_fg.get_tonemapped_color_handle();

    render_graph_builder.create_transfer_pass(
        "Present to Swapchain",
        [&](auto& builder) {
            builder.read(color_target, tempest::rhi::image_layout::transfer_src,
                         tempest::make_enum_mask(tempest::rhi::pipeline_stage::blit),
                         tempest::make_enum_mask(tempest::rhi::memory_access::transfer_read));
            builder.write(swapchain_handle, tempest::rhi::image_layout::transfer_dst,
                          tempest::make_enum_mask(tempest::rhi::pipeline_stage::blit),
                          tempest::make_enum_mask(tempest::rhi::memory_access::transfer_write));
        },
        [](tempest::graphics::transfer_task_execution_context& ctx, auto swapchain_handle, auto color) {
            ctx.blit(color, swapchain_handle);
        },
        swapchain_handle, color_target);

    pbr_fg.compile({
        .graphics_queues = 1,
        .compute_queues = 1,
        .transfer_queues = 1,
    });

    pbr_fg.upload_objects_sync(entities, mesh_registry, texture_registry, material_registry);

    auto camera = entity_registry.create();
    tempest::graphics::camera_component camera_data = {
        .aspect_ratio = 16.0F / 9.0F,
        .vertical_fov = 100.0F,
        .near_plane = 0.01F,
    };
    entity_registry.assign(camera, camera_data);
    auto camera_tx = tempest::ecs::transform_component::identity();
    camera_tx.position({0.0F, 15.0F, -1.0F});
    camera_tx.rotation({0.0F, tempest::math::as_radians(90.0F), 0.0F});
    entity_registry.assign(camera, camera_tx);

    auto sun = entity_registry.create();
    auto sun_data = tempest::graphics::directional_light_component{
        .color = {1.0F, 1.0F, 1.0F},
        .intensity = 1.0F,
    };

    auto sun_shadows = tempest::graphics::shadow_map_component{
        .shadow_distance = 2048.0F,
        .split_lambda = 0.9F,
        .blend_fraction = 0.1F,
        .cascade_count = 4,
    };

    auto sun_tx = tempest::ecs::transform_component::identity();
    sun_tx.rotation({tempest::math::as_radians(90.0F), 0.0F, 0.0F});

    entity_registry.assign_or_replace(sun, sun_shadows);
    entity_registry.assign_or_replace(sun, sun_data);
    entity_registry.assign_or_replace(sun, sun_tx);
    entity_registry.name(sun, "Sun");

    logger.trace("Application started");

    while (true)
    {
        tempest::core::input::poll();

        if (window->should_close())
        {
            break;
        }

        pbr_fg.execute();
    }

    (void)asset_database.save();

    return 0;
}