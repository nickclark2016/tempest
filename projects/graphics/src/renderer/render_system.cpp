#include <tempest/render_system.hpp>

#include <tempest/logger.hpp>

namespace tempest::graphics
{
    namespace
    {
        auto log = logger::logger_factory::create({"tempest::graphics::render_system"});
    }

    render_system::render_system(ecs::registry& entities) : _allocator{64 * 1024 * 1024}
    {
        _context = render_context::create(&_allocator);

        auto devices = _context->enumerate_suitable_devices();
        if (devices.empty())
        {
            log->critical("No suitable devices found for rendering");
            std::terminate();
        }

        log->info("Found {} suitable devices. Selecting device {}", devices.size(), devices[0].name);

        _device = &_context->create_device(0);
    }

    void render_system::register_window(iwindow& win)
    {
        auto swapchain_handle = _device->create_swapchain({
            .win = &win,
            .desired_frame_count = 3,
            .use_vsync = false,
        });

        _swapchains.emplace(&win, swapchain_handle);
    }

    void render_system::unregister_window(iwindow& win)
    {
        auto it = _swapchains.find(&win);
        if (it != _swapchains.end())
        {
            _device->release_swapchain(it->second);
            _swapchains.erase(it);
        }
    }

    void render_system::on_initialize()
    {
        auto rgc = render_graph_compiler::create_compiler(&_allocator, _device);

        auto color_buffer = rgc->create_image({
            .width = 1920,
            .height = 1080,
            .fmt = resource_format::RGBA8_SRGB,
            .type = image_type::IMAGE_2D,
            .persistent = true,
            .name = "Color Buffer",
        });

        auto default_pass =
            rgc->add_graph_pass("Default Pass", queue_operation_type::GRAPHICS, [&](graph_pass_builder& builder) {
                builder
                    .add_color_attachment(color_buffer, resource_access_type::READ_WRITE, load_op::CLEAR,
                                          store_op::STORE, {1.0f, 0.0f, 1.0f, 1.0f})
                    .on_execute([&](command_list& cmds) {});
            });

        for (auto [win, sc_handle] : _swapchains)
        {            
            auto resolve_pass = rgc->add_graph_pass(
                "Resolve Pass", queue_operation_type::GRAPHICS_AND_TRANSFER, [&](graph_pass_builder& builder) {
                    builder.add_blit_source(color_buffer)
                        .add_external_blit_target(sc_handle)
                        .depends_on(default_pass)
                        // .should_execute([&, sc_handle]() { return _swapchains.contains(win); })
                        .on_execute([&, sc_handle, color_buffer](command_list& cmds) {
                            cmds.blit(color_buffer, _device->fetch_current_image(sc_handle));
                        });
                });
        }

        _graph = std::move(*rgc).compile();

        _images.push_back(color_buffer);
    }

    void render_system::render()
    {
        _graph->execute();
    }

    void render_system::on_close()
    {
        for (auto [win, sc_handle] : _swapchains)
        {
            _device->release_swapchain(sc_handle);
        }

        for (auto img : _images)
        {
            _device->release_image(img);
        }

        for (auto buf : _buffers)
        {
            _device->release_buffer(buf);
        }

        for (auto pipeline : _graphics_pipelines)
        {
            _device->release_graphics_pipeline(pipeline);
        }

        for (auto pipeline : _compute_pipelines)
        {
            _device->release_compute_pipeline(pipeline);
        }

        for (auto sampler : _samplers)
        {
            _device->release_sampler(sampler);
        }

        _swapchains.clear();
        _images.clear();
        _buffers.clear();
        _graphics_pipelines.clear();
        _compute_pipelines.clear();
        _samplers.clear();
    }
} // namespace tempest::graphics