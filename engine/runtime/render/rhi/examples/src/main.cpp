#include "example_registry.hpp"
#include "examples/render_system_example.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <tempest/array.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/string_view.hpp>
#include <tempest/vk/context.hpp>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#if defined(TEMPEST_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#elif defined(TEMPEST_PLATFORM_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#endif

namespace
{
    using namespace tempest;
    using namespace tempest::rhi;
    using namespace tempest::rhi::examples;

    inline auto operator<<(std::ostream& os, string_view sv) -> std::ostream&
    {
        return os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
    }

    inline auto operator<<(std::ostream& os, cstring_view sv) -> std::ostream&
    {
        return os.write(sv.data(), static_cast<std::streamsize>(sv.size()));
    }

    inline auto operator<<(std::ostream& os, const string& str) -> std::ostream&
    {
        return os.write(str.data(), static_cast<std::streamsize>(str.size()));
    }

    struct cli_options
    {
        string_view example_name = "triangle";
        string_view model_name = "";
        uint32_t width = 1920;
        uint32_t height = 1080;
        uint32_t max_frames = 0;
        bool list_examples = false;
        bool show_help = false;
    };

    auto print_help(const char* prog_name) -> void
    {
        std::cout << "Tempest RHI Examples\n"
                  << "Usage: " << prog_name << " [options] [example-name]\n\n"
                  << "Options:\n"
                  << "  -l, --list             List all available examples\n"
                  << "  -e, --example <name>   Select an example to run (default: triangle)\n"
                  << "  -m, --model <name>     Select model for render_system (sponza, chess, abeautifulgame)\n"
                  << "  -w, --width <pixels>   Window width (default: 1280)\n"
                  << "  -h, --height <pixels>  Window height (default: 720)\n"
                  << "  -f, --frames <count>   Run for N frames and exit (0 = infinite)\n"
                  << "  --help                 Show this help message\n\n"
                  << "Available Examples:\n";

        for (const auto& ex : example_registry::get_examples())
        {
            std::cout << "  " << ex.name << "\t- " << ex.description << "\n";
        }
    }

    auto print_list() -> void
    {
        std::cout << "Tempest RHI Examples - Available Examples:\n";
        for (const auto& ex : example_registry::get_examples())
        {
            std::cout << "  * " << ex.name;
            if (ex.name == "triangle")
            {
                std::cout << " (Default)";
            }
            std::cout << "\n    " << ex.description << "\n";
        }
    }

    auto parse_args(int argc, char** argv) -> optional<cli_options>
    {
        auto options = cli_options{};

        for (int i = 1; i < argc; ++i)
        {
            auto arg = string_view{static_cast<const char*>(argv[i])};

            if (arg == "--list" || arg == "-l")
            {
                options.list_examples = true;
                return options;
            }
            if (arg == "--help" || arg == "-h")
            {
                options.show_help = true;
                return options;
            }
            if (arg == "--example" || arg == "-e")
            {
                if (i + 1 < argc)
                {
                    options.example_name = static_cast<const char*>(argv[++i]);
                }
                else
                {
                    std::cerr << "Error: --example requires an argument.\n";
                    return nullopt;
                }
            }
            else if (arg == "--model" || arg == "-m")
            {
                if (i + 1 < argc)
                {
                    options.model_name = static_cast<const char*>(argv[++i]);
                }
                else
                {
                    std::cerr << "Error: --model requires an argument (e.g. sponza, chess, abeautifulgame).\n";
                    return nullopt;
                }
            }
            else if (arg == "--width" || arg == "-w")
            {
                if (i + 1 < argc)
                {
                    options.width = static_cast<uint32_t>(std::atoi(argv[++i]));
                }
                else
                {
                    std::cerr << "Error: --width requires a numeric argument.\n";
                    return nullopt;
                }
            }
            else if (arg == "--height")
            {
                if (i + 1 < argc)
                {
                    options.height = static_cast<uint32_t>(std::atoi(argv[++i]));
                }
                else
                {
                    std::cerr << "Error: --height requires a numeric argument.\n";
                    return nullopt;
                }
            }
            else if (arg == "--frames" || arg == "-f")
            {
                if (i + 1 < argc)
                {
                    options.max_frames = static_cast<uint32_t>(std::atoi(argv[++i]));
                }
                else
                {
                    std::cerr << "Error: --frames requires a numeric argument.\n";
                    return nullopt;
                }
            }
            else if (!arg.empty() && arg[0] == '-')
            {
                std::cerr << "Error: Unknown option '" << arg << "'. Use --help for usage.\n";
                return nullopt;
            }
            else
            {
                // Positional example name
                options.example_name = arg;
            }
        }

        return options;
    }

    struct frame_sync
    {
        semaphore_handle acquire_sem{};
        semaphore_handle timeline_sem{};
        uint64_t timeline_value{0};
    };
} // namespace

int main(int argc, char** argv)
{
    auto parsed_opts = parse_args(argc, argv);
    if (!parsed_opts.has_value())
    {
        return 1;
    }

    auto opts = *parsed_opts;

    if (opts.show_help)
    {
        print_help(argv[0]);
        return 0;
    }

    if (opts.list_examples)
    {
        print_list();
        return 0;
    }

    auto example_meta = example_registry::find_example(opts.example_name);
    if (!example_meta.has_value())
    {
        std::cerr << "Error: Unknown example '" << opts.example_name << "'.\n\n";
        print_list();
        return 1;
    }

    std::cout << "Starting Tempest RHI Example: " << example_meta->name << "\n"
              << "Description: " << example_meta->description << "\n";

    // 1. Initialize GLFW
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    auto title = "Tempest RHI - " + std::string(example_meta->name.data(), example_meta->name.size());
    auto* window = glfwCreateWindow(static_cast<int>(opts.width), static_cast<int>(opts.height), title.c_str(),
                                    nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        return 1;
    }

    // 2. Create RHI Context and Device
    auto ctx_desc = context_desc{
        .application_name = "Tempest RHI Examples",
        .version_major = 1,
        .version_minor = 0,
        .version_patch = 0,
        .enable_api_validation = true,
        .api = graphics_api::vulkan,
    };

    auto ctx_res = vk::create_context(ctx_desc);
    if (!ctx_res.has_value())
    {
        std::cerr << "Failed to create RHI context.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    auto context = tempest::move(ctx_res).value();
    auto devices = context->enumerate_devices();
    if (devices.empty())
    {
        std::cerr << "No compatible GPU devices found.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    std::cout << "Using GPU: " << devices[0].name << "\n";
    auto dev = context->create_device(devices[0].device_uuid);
    if (!dev)
    {
        std::cerr << "Failed to create RHI device.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // 3. Create WSI Surface & Negotiate sRGB Swapchain Format
#if defined(TEMPEST_PLATFORM_WINDOWS)
    auto native_handle = native_wsi_handle{
        .display = GetModuleHandle(nullptr),
        .window = glfwGetWin32Window(window),
    };
#elif defined(TEMPEST_PLATFORM_LINUX)
    auto native_handle = native_wsi_handle{
        .display = static_cast<void*>(glfwGetX11Display()),
        .window = reinterpret_cast<void*>(static_cast<uintptr_t>(glfwGetX11Window(window))),
    };
#else
    auto native_handle = native_wsi_handle{};
#endif

    auto raw_res = dev->create_raw_surface(native_handle);
    if (!raw_res.has_value())
    {
        std::cerr << "Failed to create raw surface.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    auto raw_surf = raw_res.value();

    auto caps = dev->get_surface_capabilities(raw_surf);
    if (caps.supported_formats.empty() || caps.supported_present_modes.empty())
    {
        std::cerr << "Surface capabilities query returned no formats or present modes.\n";
        dev->destroy_raw_surface(raw_surf);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // Negotiate sRGB non-linear color space with bgra8_srgb or rgba8_srgb
    auto selected_surface_format = optional<surface_format>{};
    for (const auto& fmt : caps.supported_formats)
    {
        if (fmt.color_space == surface_color_space::srgb_nonlinear &&
            (fmt.format == render_surface_format::bgra8_srgb || fmt.format == render_surface_format::rgba8_srgb))
        {
            selected_surface_format = fmt;
            break;
        }
    }
    if (!selected_surface_format)
    {
        // Fallback to first available format
        selected_surface_format = caps.supported_formats[0];
    }

    auto selected_present_mode = present_mode::vsync;
    for (const auto& mode : caps.supported_present_modes)
    {
        if (mode == present_mode::vsync)
        {
            selected_present_mode = mode;
            break;
        }
    }

    std::cout << "Selected Surface Format: "
              << (selected_surface_format->format == render_surface_format::bgra8_srgb   ? "BGRA8_SRGB"
                  : selected_surface_format->format == render_surface_format::rgba8_srgb ? "RGBA8_SRGB"
                  : selected_surface_format->format == render_surface_format::bgra8_unorm
                      ? "BGRA8_UNORM"
                      : "RGBA8_UNORM")
              << " (sRGB Non-linear)\n";

    auto surf_desc = render_surface_desc{
        .raw_surface = raw_surf,
        .present_mode = selected_present_mode,
        .format = *selected_surface_format,
        .width = opts.width,
        .height = opts.height,
        .min_image_count = caps.min_image_count,
        .preferred_image_count = caps.min_image_count + 1,
    };

    auto surface = dev->create_render_surface(surf_desc);
    if (!surface)
    {
        std::cerr << "Failed to create render surface.\n";
        dev->destroy_raw_surface(raw_surf);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // 4. Initialize Example Instance
    if (!opts.model_name.empty())
    {
        if (opts.model_name == "chess" || opts.model_name == "abeautifulgame")
        {
            render_system_example::set_default_model(scene_model::chess);
        }
        else if (opts.model_name == "sponza")
        {
            render_system_example::set_default_model(scene_model::sponza);
        }
    }

    auto example_instance = example_meta->factory();
    if (!example_instance->init(*dev, surface->get_format()))
    {
        std::cerr << "Failed to initialize example '" << example_meta->name << "'.\n";
        dev->destroy_render_surface(tempest::move(surface));
        dev->destroy_raw_surface(raw_surf);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    // 5. Setup Synchronization Objects
    // - Acquire and timeline semaphores are per frame in flight
    // - Render (present) semaphores are indexed per swapchain image to ensure safe reuse
    constexpr auto max_frames_in_flight = size_t{2};
    auto frame_syncs = array<frame_sync, max_frames_in_flight>{};
    for (auto& sync : frame_syncs)
    {
        sync.acquire_sem = dev->create_binary_semaphore();
        sync.timeline_sem = dev->create_timeline_semaphore();
        sync.timeline_value = 0;
    }

    auto render_semaphores = vector<semaphore_handle>{};
    auto get_render_semaphore = [&](uint32_t image_index) -> semaphore_handle {
        while (image_index >= render_semaphores.size())
        {
            render_semaphores.push_back(dev->create_binary_semaphore());
        }
        return render_semaphores[image_index];
    };

    auto& graphics_port = dev->get_graphics_execution_port();
    auto frame_index = uint64_t{0};
    auto need_recreate = false;

    // 6. Main Render Loop
    while (glfwWindowShouldClose(window) == GLFW_FALSE && (opts.max_frames == 0 || frame_index < opts.max_frames))
    {
        glfwPollEvents();

        auto cur_w = int{0};
        auto cur_h = int{0};
        glfwGetFramebufferSize(window, &cur_w, &cur_h);

        if (cur_w == 0 || cur_h == 0)
        {
            // Window is minimized
            continue;
        }

        if (static_cast<uint32_t>(cur_w) != surface->get_width() ||
            static_cast<uint32_t>(cur_h) != surface->get_height() || need_recreate)
        {
            dev->wait_idle();

            auto new_caps = dev->get_surface_capabilities(raw_surf);
            auto new_surf_desc = render_surface_desc{
                .raw_surface = raw_surf,
                .present_mode = selected_present_mode,
                .format = *selected_surface_format,
                .width = static_cast<uint32_t>(cur_w),
                .height = static_cast<uint32_t>(cur_h),
                .min_image_count = new_caps.min_image_count,
                .preferred_image_count = new_caps.min_image_count + 1,
                .old_surface = surface.get(),
            };

            auto new_surf = dev->create_render_surface(new_surf_desc);
            if (new_surf)
            {
                dev->destroy_render_surface(tempest::move(surface));
                surface = tempest::move(new_surf);
                example_instance->on_resize(*dev, surface->get_format(), surface->get_width(), surface->get_height());
                need_recreate = false;
            }
            else
            {
                continue;
            }
        }

        auto& sync = frame_syncs[frame_index % max_frames_in_flight];

        // Wait for previous frame using this flight slot to finish
        if (sync.timeline_value > 0)
        {
            dev->wait_for_sync(host_sync_point{
                .semaphore = sync.timeline_sem,
                .value = sync.timeline_value,
            });
        }

        // Acquire next swapchain image
        auto acquire_res = surface->acquire_next_image(device_sync_point{
            .semaphore = sync.acquire_sem,
            .value = 0,
            .stages = pipeline_stage::attachment_output,
        });

        if (!acquire_res.has_value())
        {
            if (acquire_res.error() == swapchain_error::out_of_date ||
                acquire_res.error() == swapchain_error::suboptimal)
            {
                need_recreate = true;
            }
            continue;
        }

        auto sc_img = acquire_res.value();

        auto render_sem = get_render_semaphore(sc_img.swapchain_image_index);
        sync.timeline_value = frame_index + 1;

        const auto frame_info = frame_render_info{
            .dev = *dev,
            .swapchain_texture = sc_img.texture,
            .swapchain_view = sc_img.view,
            .width = surface->get_width(),
            .height = surface->get_height(),
            .acquire_semaphore = sync.acquire_sem,
            .render_semaphore = render_sem,
            .timeline_semaphore = sync.timeline_sem,
            .timeline_value = sync.timeline_value,
        };

        example_instance->render(frame_info);

        // Present swapchain image
        auto present_res = surface->present(graphics_port, device_sync_point{
                                                               .semaphore = render_sem,
                                                               .value = 0,
                                                           });
        if (!present_res.has_value() && (present_res.error() == swapchain_error::out_of_date ||
                                         present_res.error() == swapchain_error::suboptimal))
        {
            need_recreate = true;
        }

        frame_index++;
    }

    // 7. Cleanup & Shutdown
    dev->wait_idle();

    example_instance->shutdown(*dev);

    for (auto& sync : frame_syncs)
    {
        dev->destroy_semaphore(sync.acquire_sem);
        dev->destroy_semaphore(sync.timeline_sem);
    }

    for (auto sem : render_semaphores)
    {
        dev->destroy_semaphore(sem);
    }

    dev->destroy_render_surface(tempest::move(surface));
    dev->destroy_raw_surface(raw_surf);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
