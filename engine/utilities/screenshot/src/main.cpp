#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#pragma clang diagnostic pop

#include <tempest/asset_database.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/default_importers.hpp>
#include <tempest/format.hpp>
#include <tempest/frame_graph.hpp>
#include <tempest/int.hpp>
#include <tempest/logger.hpp>
#include <tempest/math.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/print.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>
#include <tempest/string.hpp>
#include <tempest/string_view.hpp>
#include <tempest/traits.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/vector.hpp>

namespace tempest::rhi::vk
{
    unique_ptr<rhi::instance> create_instance(tempest::logger* log, bool headless) noexcept;
} // namespace tempest::rhi::vk

namespace
{
    auto parse_u32(tempest::string_view s) -> tempest::uint32_t
    {
        auto val = tempest::uint32_t{0};
        for (auto ch : s)
        {
            if (ch >= '0' && ch <= '9')
            {
                val = (val * 10) + static_cast<tempest::uint32_t>(ch - '0');
            }
            else
            {
                break;
            }
        }
        return val;
    }

    auto parse_f32(tempest::string_view s) -> float
    {
        auto idx = tempest::size_t{0};
        const auto len = s.size();

        while (idx < len && (s[idx] == ' ' || s[idx] == '\t'))
        {
            ++idx;
        }

        auto sign = 1.0F;
        if (idx < len)
        {
            if (s[idx] == '-')
            {
                sign = -1.0F;
                ++idx;
            }
            else if (s[idx] == '+')
            {
                ++idx;
            }
        }

        auto result = 0.0;
        while (idx < len && s[idx] >= '0' && s[idx] <= '9')
        {
            result = (result * 10.0) + static_cast<double>(s[idx] - '0');
            ++idx;
        }

        if (idx < len && s[idx] == '.')
        {
            ++idx;
            auto factor = 0.1;
            while (idx < len && s[idx] >= '0' && s[idx] <= '9')
            {
                result += static_cast<double>(s[idx] - '0') * factor;
                factor *= 0.1;
                ++idx;
            }
        }

        if (idx < len && (s[idx] == 'e' || s[idx] == 'E'))
        {
            ++idx;
            auto exp_sign = 1;
            if (idx < len)
            {
                if (s[idx] == '-')
                {
                    exp_sign = -1;
                    ++idx;
                }
                else if (s[idx] == '+')
                {
                    ++idx;
                }
            }
            auto exp = 0;
            while (idx < len && s[idx] >= '0' && s[idx] <= '9')
            {
                exp = (exp * 10) + (s[idx] - '0');
                ++idx;
            }
            auto scale = 1.0;
            for (auto e = 0; e < exp; ++e)
            {
                scale *= 10.0;
            }
            if (exp_sign < 0)
            {
                result /= scale;
            }
            else
            {
                result *= scale;
            }
        }

        return static_cast<float>(sign * result);
    }

    struct cli_args
    {
        const char* asset_path = nullptr;
        const char* output_base = nullptr;
        tempest::uint32_t wait_frames = 5;
        tempest::uint32_t capture_frames = 1;
        tempest::uint32_t width = 1920;
        tempest::uint32_t height = 1080;

        // Asset transform (identity by default)
        float pos_x = 0.0F, pos_y = 0.0F, pos_z = 0.0F;
        float rot_x = 0.0F, rot_y = 0.0F, rot_z = 0.0F; // degrees
        float scale = 1.0F;

        // Camera (matching sandbox defaults)
        float cam_pos_x = 0.0F, cam_pos_y = 15.0F, cam_pos_z = -1.0F;
        float cam_rot_x = 0.0F, cam_rot_y = 90.0F, cam_rot_z = 0.0F; // degrees
    };

    auto print_usage(const char* exe) -> void
    {
        tempest::print_to(tempest::stderr_stream,
                          "Usage: {} --asset <path> --output <base> [options]\n"
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

    auto parse_args(int argc, char** argv, cli_args& out) -> bool
    {
        for (auto i = 1; i < argc; ++i)
        {
            auto arg = tempest::string_view{argv[i]};

            if (arg == "--asset" && i + 1 < argc)
            {
                out.asset_path = argv[++i];
            }
            else if (arg == "--output" && i + 1 < argc)
            {
                out.output_base = argv[++i];
            }
            else if (arg == "--wait-frames" && i + 1 < argc)
            {
                out.wait_frames = parse_u32(argv[++i]);
            }
            else if (arg == "--capture-frames" && i + 1 < argc)
            {
                out.capture_frames = parse_u32(argv[++i]);
            }
            else if (arg == "--width" && i + 1 < argc)
            {
                out.width = parse_u32(argv[++i]);
            }
            else if (arg == "--height" && i + 1 < argc)
            {
                out.height = parse_u32(argv[++i]);
            }
            else if (arg == "--position" && i + 3 < argc)
            {
                out.pos_x = parse_f32(argv[++i]);
                out.pos_y = parse_f32(argv[++i]);
                out.pos_z = parse_f32(argv[++i]);
            }
            else if (arg == "--rotation" && i + 3 < argc)
            {
                out.rot_x = parse_f32(argv[++i]);
                out.rot_y = parse_f32(argv[++i]);
                out.rot_z = parse_f32(argv[++i]);
            }
            else if (arg == "--scale" && i + 1 < argc)
            {
                out.scale = parse_f32(argv[++i]);
            }
            else if (arg == "--camera-position" && i + 3 < argc)
            {
                out.cam_pos_x = parse_f32(argv[++i]);
                out.cam_pos_y = parse_f32(argv[++i]);
                out.cam_pos_z = parse_f32(argv[++i]);
            }
            else if (arg == "--camera-rotation" && i + 3 < argc)
            {
                out.cam_rot_x = parse_f32(argv[++i]);
                out.cam_rot_y = parse_f32(argv[++i]);
                out.cam_rot_z = parse_f32(argv[++i]);
            }
            else
            {
                tempest::println_to(tempest::stderr_stream, "Unknown or incomplete argument: {}", arg);
                return false;
            }
        }
        return true;
    }
} // namespace

auto main(int argc, char** argv) -> int
{
    auto args = cli_args{};
    if (!parse_args(argc, argv, args))
    {
        print_usage(argv[0]);
        return 1;
    }

    if ((args.asset_path == nullptr) || (args.output_base == nullptr))
    {
        tempest::println_to(tempest::stderr_stream, "Error: --asset and --output are required.\n");
        print_usage(argv[0]);
        return 1;
    }

    if (args.width == 0 || args.height == 0)
    {
        tempest::println_to(tempest::stderr_stream, "Error: --width and --height must be non-zero.");
        return 1;
    }

    if (args.capture_frames == 0)
    {
        tempest::println_to(tempest::stderr_stream, "Error: --capture-frames must be at least 1.");
        return 1;
    }

    auto stdout_sink = tempest::stdout_log_sink();
    auto logger = tempest::logger(stdout_sink);

    // Headless Vulkan instance — no window surface or swapchain
    auto instance = tempest::rhi::vk::create_instance(&logger, /*headless=*/true);
    if (!instance)
    {
        tempest::println_to(tempest::stderr_stream, "Error: Failed to create Vulkan instance.");
        return 1;
    }

    auto& device = instance->acquire_device(0);

    // Registries
    auto event_registry = tempest::event::event_registry();
    auto entity_registry = tempest::ecs::archetype_registry(event_registry);
    auto mesh_registry = tempest::core::mesh_registry();
    auto texture_registry = tempest::core::texture_registry();
    auto material_registry = tempest::core::material_registry();

    // Asset database — no .tassetdb file, import directly from disk
    auto asset_type_reg = tempest::assets::asset_type_registry();
    auto asset_db = tempest::assets::asset_database(&asset_type_reg);
    tempest::assets::register_default_importers(asset_db, &mesh_registry, &texture_registry, &material_registry);

    const auto prefab = asset_db.load(args.asset_path, entity_registry);
    if (prefab == tempest::ecs::tombstone)
    {
        tempest::println_to(
            tempest::stderr_stream,
            "Error: Failed to load asset '{}'. Check that the path is correct and the file format is supported.",
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

    auto entities = tempest::vector<tempest::ecs::entity>{};
    entities.push_back(asset_entity);

    // Build the PBR frame graph — rgba8_unorm tonemapped output avoids any channel swap
    auto pbr_fg =
        tempest::graphics::pbr_frame_graph(device,
                                           {
                                               .render_target_width = args.width,
                                               .render_target_height = args.height,
                                               .hdr_color_format = tempest::rhi::image_format::rgba16_float,
                                               .depth_format = tempest::rhi::image_format::d32_float,
                                               .tonemapped_color_format = tempest::rhi::image_format::rgba8_srgb,
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

    // Readback buffer: one pixel is 4 bytes (rgba8_unorm)
    const auto readback_size = static_cast<tempest::size_t>(args.width) * args.height * 4;
    const auto readback_buf = device.create_buffer({
        .size = readback_size,
        .location = tempest::rhi::memory_location::host,
        .usage = tempest::make_enum_mask(tempest::rhi::buffer_usage::transfer_dst),
        .access_type = tempest::rhi::host_access_type::coherent,
        .access_pattern = tempest::rhi::host_access_pattern::random,
        .name = "screenshot_readback",
    });

    // Import the readback buffer and build the screenshot readback pass
    auto render_graph_builder_opt = pbr_fg.get_builder();
    auto& builder = render_graph_builder_opt.value();

    auto readback_handle = builder.import_buffer("Screenshot Readback Buffer", readback_buf);
    auto color_handle = pbr_fg.get_tonemapped_color_handle();

    builder.create_transfer_pass(
        "Screenshot Readback",
        [&](auto& task_builder) -> auto {
            task_builder.read(color_handle, tempest::rhi::image_layout::transfer_src,
                              tempest::make_enum_mask(tempest::rhi::pipeline_stage::copy),
                              tempest::make_enum_mask(tempest::rhi::memory_access::transfer_read));
            task_builder.write(readback_handle, tempest::make_enum_mask(tempest::rhi::pipeline_stage::copy),
                               tempest::make_enum_mask(tempest::rhi::memory_access::transfer_write));
        },
        [](tempest::graphics::transfer_task_execution_context& ctx, auto rb_handle, auto color) -> auto {
            ctx.copy_image_to_buffer(color, rb_handle);
        },
        readback_handle, color_handle);

    pbr_fg.compile({
        .graphics_queues = 1,
        .compute_queues = 1,
        .transfer_queues = 1,
    });

    pbr_fg.upload_objects_sync(entities, mesh_registry, texture_registry, material_registry);

    // Camera entity
    auto camera = entity_registry.create();
    tempest::graphics::camera_component camera_data = {
        .aspect_ratio = static_cast<float>(args.width) / static_cast<float>(args.height),
        .vertical_fov = 100.0F,
        .near_plane = 0.01F,
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
                                               .color = {1.0f, 1.0f, 1.0f},
                                               .intensity = 3.0f,
                                           });
    entity_registry.assign_or_replace(sun, tempest::graphics::shadow_map_component{
                                               .shadow_distance = 2048.0f,
                                               .split_lambda = 0.9f,
                                               .blend_fraction = 0.1f,
                                               .cascade_count = 4,
                                           });
    auto sun_tx = tempest::ecs::transform_component::identity();
    sun_tx.rotation({tempest::math::as_radians(90.0F), 0.0F, 0.0F});
    entity_registry.assign_or_replace(sun, sun_tx);
    entity_registry.name(sun, "Sun");

    // Main render + capture loop
    const auto total_frames = args.wait_frames + args.capture_frames;
    auto captures = tempest::uint32_t{0};

    for (auto i = tempest::uint32_t{0}; i < total_frames; ++i)
    {
        pbr_fg.execute();

        if (i >= args.wait_frames)
        {
            device.wait_idle();

            auto* data = device.map_buffer(readback_buf);
            if (!data)
            {
                tempest::println_to(tempest::stderr_stream, "Error: Failed to map readback buffer for frame {}.",
                                    captures);
            }
            else
            {
                const auto filename = tempest::format("{}_{:04d}.png", args.output_base, captures);
                const auto written =
                    stbi_write_png(filename.c_str(), static_cast<int>(args.width), static_cast<int>(args.height), 4,
                                   data, static_cast<int>(args.width) * 4);

                if (written == 0)
                {
                    tempest::println_to(tempest::stderr_stream, "Error: stbi_write_png failed for '{}'.", filename);
                }
                else
                {
                    tempest::println("Captured: {}", filename);
                }

                device.unmap_buffer(readback_buf);
            }

            ++captures;
        }
    }

    return 0;
}
