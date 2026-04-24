#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma clang diagnostic pop

#include <tempest/asset_database.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/frame_graph.hpp>
#include <tempest/logger.hpp>
#include <tempest/math.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/traits.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>

namespace tempest::rhi::vk
{
    unique_ptr<rhi::instance> create_instance(tempest::logger* log, bool headless) noexcept;
} // namespace tempest::rhi::vk

namespace
{
    struct cli_args
    {
        const char* asset_path    = nullptr;
        const char* output_base   = nullptr;
        uint32_t    wait_frames   = 5;
        uint32_t    capture_frames = 1;
        uint32_t    width  = 1920;
        uint32_t    height = 1080;

        // Asset transform (identity by default)
        float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
        float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f; // degrees
        float scale = 1.0f;

        // Camera (matching sandbox defaults)
        float cam_pos_x = 0.0f,  cam_pos_y = 15.0f, cam_pos_z = -1.0f;
        float cam_rot_x = 0.0f,  cam_rot_y = 90.0f, cam_rot_z = 0.0f;  // degrees
    };

    void print_usage(const char* exe)
    {
        std::fprintf(stderr,
            "Usage: %s --asset <path> --output <base> [options]\n"
            "\n"
            "Required:\n"
            "  --asset  <path>      Path to the asset file to load (e.g. path/to/model.gltf)\n"
            "  --output <base>      Output base path; frames written as <base>_NNNN.png\n"
            "\n"
            "Optional:\n"
            "  --wait-frames    <N>     Warm-up frames before capturing (default: 5)\n"
            "  --capture-frames <N>     Number of frames to capture (default: 1)\n"
            "  --width  <W>             Render width  (default: 1920)\n"
            "  --height <H>             Render height (default: 1080)\n"
            "  --position <x> <y> <z>  Asset position (default: 0 0 0)\n"
            "  --rotation <x> <y> <z>  Asset rotation in degrees (default: 0 0 0)\n"
            "  --scale  <s>             Asset uniform scale (default: 1.0)\n"
            "  --camera-position <x> <y> <z>  Camera position (default: 0 15 -1)\n"
            "  --camera-rotation <x> <y> <z>  Camera rotation in degrees (default: 0 90 0)\n",
            exe);
    }

    bool parse_args(int argc, char** argv, cli_args& out)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::strcmp(argv[i], "--asset") == 0 && i + 1 < argc)
            {
                out.asset_path = argv[++i];
            }
            else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            {
                out.output_base = argv[++i];
            }
            else if (std::strcmp(argv[i], "--wait-frames") == 0 && i + 1 < argc)
            {
                out.wait_frames = static_cast<uint32_t>(std::atoi(argv[++i]));
            }
            else if (std::strcmp(argv[i], "--capture-frames") == 0 && i + 1 < argc)
            {
                out.capture_frames = static_cast<uint32_t>(std::atoi(argv[++i]));
            }
            else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc)
            {
                out.width = static_cast<uint32_t>(std::atoi(argv[++i]));
            }
            else if (std::strcmp(argv[i], "--height") == 0 && i + 1 < argc)
            {
                out.height = static_cast<uint32_t>(std::atoi(argv[++i]));
            }
            else if (std::strcmp(argv[i], "--position") == 0 && i + 3 < argc)
            {
                out.pos_x = std::stof(argv[++i]);
                out.pos_y = std::stof(argv[++i]);
                out.pos_z = std::stof(argv[++i]);
            }
            else if (std::strcmp(argv[i], "--rotation") == 0 && i + 3 < argc)
            {
                out.rot_x = std::stof(argv[++i]);
                out.rot_y = std::stof(argv[++i]);
                out.rot_z = std::stof(argv[++i]);
            }
            else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc)
            {
                out.scale = std::stof(argv[++i]);
            }
            else if (std::strcmp(argv[i], "--camera-position") == 0 && i + 3 < argc)
            {
                out.cam_pos_x = std::stof(argv[++i]);
                out.cam_pos_y = std::stof(argv[++i]);
                out.cam_pos_z = std::stof(argv[++i]);
            }
            else if (std::strcmp(argv[i], "--camera-rotation") == 0 && i + 3 < argc)
            {
                out.cam_rot_x = std::stof(argv[++i]);
                out.cam_rot_y = std::stof(argv[++i]);
                out.cam_rot_z = std::stof(argv[++i]);
            }
            else
            {
                std::fprintf(stderr, "Unknown or incomplete argument: %s\n", argv[i]);
                return false;
            }
        }
        return true;
    }
} // namespace

auto main(int argc, char** argv) -> int
{
    cli_args args;
    if (!parse_args(argc, argv, args))
    {
        print_usage(argv[0]);
        return 1;
    }

    if (!args.asset_path || !args.output_base)
    {
        std::fprintf(stderr, "Error: --asset and --output are required.\n\n");
        print_usage(argv[0]);
        return 1;
    }

    if (args.width == 0 || args.height == 0)
    {
        std::fprintf(stderr, "Error: --width and --height must be non-zero.\n");
        return 1;
    }

    if (args.capture_frames == 0)
    {
        std::fprintf(stderr, "Error: --capture-frames must be at least 1.\n");
        return 1;
    }

    auto stdout_sink = tempest::stdout_log_sink();
    auto logger = tempest::logger(stdout_sink);

    // Headless Vulkan instance — no window surface or swapchain
    auto instance = tempest::rhi::vk::create_instance(&logger, /*headless=*/true);
    if (!instance)
    {
        std::fprintf(stderr, "Error: Failed to create Vulkan instance.\n");
        return 1;
    }

    auto& device = instance->acquire_device(0);

    // Registries
    auto event_registry    = tempest::event::event_registry();
    auto entity_registry   = tempest::ecs::archetype_registry(event_registry);
    auto mesh_registry     = tempest::core::mesh_registry();
    auto texture_registry  = tempest::core::texture_registry();
    auto material_registry = tempest::core::material_registry();

    // Asset database — no .tassetdb file, import directly from disk
    auto asset_type_reg  = tempest::assets::asset_type_registry();
    auto asset_db        = tempest::assets::asset_database(&asset_type_reg);
    tempest::assets::register_default_importers(asset_db, &mesh_registry, &texture_registry, &material_registry);

    const auto prefab = asset_db.load(args.asset_path, entity_registry);
    if (prefab == tempest::ecs::tombstone)
    {
        std::fprintf(stderr, "Error: Failed to load asset '%s'. Check that the path is correct and the file format is supported.\n",
                     args.asset_path);
        return 1;
    }

    // Instantiate the loaded prefab and apply the CLI transform
    const auto asset_entity = entity_registry.duplicate(prefab);
    auto asset_tx = tempest::ecs::transform_component::identity();
    asset_tx.position({args.pos_x, args.pos_y, args.pos_z});
    asset_tx.rotation({
        tempest::math::as_radians(args.rot_x),
        tempest::math::as_radians(args.rot_y),
        tempest::math::as_radians(args.rot_z),
    });
    asset_tx.scale({args.scale, args.scale, args.scale});
    entity_registry.assign_or_replace(asset_entity, asset_tx);

    auto entities = tempest::vector<tempest::ecs::archetype_entity>{};
    entities.push_back(asset_entity);

    // Build the PBR frame graph — rgba8_unorm tonemapped output avoids any channel swap
    auto pbr_fg = tempest::graphics::pbr_frame_graph(
        device,
        {
            .render_target_width  = args.width,
            .render_target_height = args.height,
            .hdr_color_format        = tempest::rhi::image_format::rgba16_float,
            .depth_format            = tempest::rhi::image_format::d32_float,
            .tonemapped_color_format = tempest::rhi::image_format::rgba8_srgb,
            .vertex_data_buffer_size       = 16 * 1024 * 1024,
            .max_mesh_count                = 16 * 1024 * 1024,
            .max_material_count            = 4 * 1024 * 1024,
            .staging_buffer_size_per_frame = 16 * 1024 * 1024,
            .max_object_count              = 256 * 1024,
            .max_lights                    = 256,
            .max_bindless_textures         = 1024,
            .max_anisotropy                = 16.0f,
            .light_clustering =
                {
                    .cluster_count_x       = 16,
                    .cluster_count_y       = 9,
                    .cluster_count_z       = 24,
                    .max_lights_per_cluster = 128,
                },
            .shadows =
                {
                    .directional_shadow_map_width  = 16384,
                    .directional_shadow_map_height = 8192,
                    .max_shadow_casting_lights     = 16,
                },
        },
        {
            .entity_registry = &entity_registry,
        });

    // Readback buffer: one pixel is 4 bytes (rgba8_unorm)
    const size_t readback_size = static_cast<size_t>(args.width) * args.height * 4;
    const auto readback_buf = device.create_buffer({
        .size         = readback_size,
        .location     = tempest::rhi::memory_location::host,
        .usage        = tempest::make_enum_mask(tempest::rhi::buffer_usage::transfer_dst),
        .access_type  = tempest::rhi::host_access_type::coherent,
        .access_pattern = tempest::rhi::host_access_pattern::random,
        .name         = "screenshot_readback",
    });

    // Import the readback buffer and build the screenshot readback pass
    auto render_graph_builder_opt = pbr_fg.get_builder();
    auto& builder = render_graph_builder_opt.value();

    auto readback_handle = builder.import_buffer("Screenshot Readback Buffer", readback_buf);
    auto color_handle     = pbr_fg.get_tonemapped_color_handle();

    builder.create_transfer_pass(
        "Screenshot Readback",
        [&](auto& task_builder) {
            task_builder.read(color_handle,
                              tempest::rhi::image_layout::transfer_src,
                              tempest::make_enum_mask(tempest::rhi::pipeline_stage::copy),
                              tempest::make_enum_mask(tempest::rhi::memory_access::transfer_read));
            task_builder.write(readback_handle,
                               tempest::make_enum_mask(tempest::rhi::pipeline_stage::copy),
                               tempest::make_enum_mask(tempest::rhi::memory_access::transfer_write));
        },
        [](tempest::graphics::transfer_task_execution_context& ctx, auto rb_handle, auto color) {
            ctx.copy_image_to_buffer(color, rb_handle);
        },
        readback_handle, color_handle);

    pbr_fg.compile({
        .graphics_queues  = 1,
        .compute_queues   = 1,
        .transfer_queues  = 1,
    });

    pbr_fg.upload_objects_sync(entities, mesh_registry, texture_registry, material_registry);

    // Camera entity
    auto camera = entity_registry.create();
    tempest::graphics::camera_component camera_data = {
        .aspect_ratio = static_cast<float>(args.width) / static_cast<float>(args.height),
        .vertical_fov = 100.0f,
        .near_plane   = 0.01f,
    };
    entity_registry.assign(camera, camera_data);
    auto camera_tx = tempest::ecs::transform_component::identity();
    camera_tx.position({args.cam_pos_x, args.cam_pos_y, args.cam_pos_z});
    camera_tx.rotation({
        tempest::math::as_radians(args.cam_rot_x),
        tempest::math::as_radians(args.cam_rot_y),
        tempest::math::as_radians(args.cam_rot_z),
    });
    entity_registry.assign(camera, camera_tx);

    // Sun entity (same config as sandbox)
    auto sun = entity_registry.create();
    entity_registry.assign_or_replace(sun, tempest::graphics::directional_light_component{
        .color     = {1.0f, 1.0f, 1.0f},
        .intensity = 3.0f,
    });
    entity_registry.assign_or_replace(sun, tempest::graphics::shadow_map_component{
        .shadow_distance = 2048.0f,
        .split_lambda    = 0.9f,
        .blend_fraction  = 0.1f,
        .cascade_count   = 4,
    });
    auto sun_tx = tempest::ecs::transform_component::identity();
    sun_tx.rotation({tempest::math::as_radians(90.0f), 0.0f, 0.0f});
    entity_registry.assign_or_replace(sun, sun_tx);
    entity_registry.name(sun, "Sun");

    // Main render + capture loop
    const uint32_t total_frames = args.wait_frames + args.capture_frames;
    uint32_t       captures     = 0;

    for (uint32_t i = 0; i < total_frames; ++i)
    {
        pbr_fg.execute();

        if (i >= args.wait_frames)
        {
            device.wait_idle();

            auto* data = device.map_buffer(readback_buf);
            if (!data)
            {
                std::fprintf(stderr, "Error: Failed to map readback buffer for frame %u.\n", captures);
            }
            else
            {
                const std::string filename = std::format("{}_{:04d}.png", args.output_base, captures);
                const int written = stbi_write_png(
                    filename.c_str(),
                    static_cast<int>(args.width),
                    static_cast<int>(args.height),
                    4,
                    data,
                    static_cast<int>(args.width) * 4);

                if (!written)
                {
                    std::fprintf(stderr, "Error: stbi_write_png failed for '%s'.\n", filename.c_str());
                }
                else
                {
                    std::fprintf(stdout, "Captured: %s\n", filename.c_str());
                }

                device.unmap_buffer(readback_buf);
            }

            ++captures;
        }
    }

    return 0;
}
