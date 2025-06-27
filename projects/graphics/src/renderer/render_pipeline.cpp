#include <tempest/render_pipeline.hpp>

namespace tempest::graphics
{
    renderer::renderer()
    {
        _rhi_instance = rhi::vk::create_instance();
        _rhi_device = &_rhi_instance->acquire_device(0);
    }

    unique_ptr<rhi::window_surface> renderer::create_window(const rhi::window_surface_desc& desc)
    {
        return rhi::vk::create_window_surface(desc);
    }

    render_pipeline* renderer::register_window(rhi::window_surface* window, unique_ptr<render_pipeline> pipeline)
    {
        auto render_surface = _rhi_device->create_render_surface({
            .window = window,
            .min_image_count = 2,
            .format =
                {
                    .space = rhi::color_space::srgb_nonlinear,
                    .format = rhi::image_format::bgra8_srgb,
                },
            .present_mode = rhi::present_mode::immediate,
            .width = window->framebuffer_width(),
            .height = window->framebuffer_height(),
            .layers = 1,
        });

        window->register_resize_callback(
            [render_surface, this]([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) {
                auto window_payload_it =
                    tempest::find_if(_windows.begin(), _windows.end(), [render_surface](const auto& payload) {
                        return payload.render_surface == render_surface;
                    });
                if (window_payload_it != _windows.end())
                {
                    window_payload_it->framebuffer_resized = true;
                }
            });

        _windows.push_back({
            .win = window,
            .render_surface = render_surface,
            .pipeline = move(pipeline),
            .framebuffer_resized = false,
        });

        _windows.back().pipeline->initialize(*this, *_rhi_device);

        // On window close, unregister the window
        window->register_close_callback([this, window]() { unregister_window(window); });

        return _windows.back().pipeline.get();
    }

    void renderer::unregister_window(rhi::window_surface* window)
    {
        auto it = tempest::find_if(_windows.begin(), _windows.end(),
                                   [window](const auto& payload) { return payload.win == window; });
        if (it != _windows.end())
        {
            it->pipeline->destroy(*this, *_rhi_device);
            _windows.erase(it);
        }
    }

    bool renderer::render()
    {
        _rhi_device->start_frame();

        for (auto it = _windows.begin(); it != _windows.end();)
        {
            auto& window = *it->win;

            if (window.should_close())
            {
                it->pipeline->destroy(*this, *_rhi_device);
                it = _windows.erase(it);
                continue;
            }

            if (window.minimized())
            {
                ++it;
                continue;
            }

            auto& pipeline = *it->pipeline;

            auto acquire_result = _rhi_device->acquire_next_image(it->render_surface);
            if (!acquire_result)
            {
                auto error_code = acquire_result.error();
                if (error_code == rhi::swapchain_error_code::out_of_date)
                {
                    _rhi_device->recreate_render_surface(it->render_surface,
                                                         {
                                                             .window = it->win,
                                                             .min_image_count = 2,
                                                             .format =
                                                                 {
                                                                     .space = rhi::color_space::srgb_nonlinear,
                                                                     .format = rhi::image_format::bgra8_srgb,
                                                                 },
                                                             .present_mode = rhi::present_mode::immediate,
                                                             .width = window.framebuffer_width(),
                                                             .height = window.framebuffer_height(),
                                                             .layers = 1,
                                                         });
                    continue;
                }
                else if (error_code == rhi::swapchain_error_code::failure)
                {
                    it->pipeline->destroy(*this, *_rhi_device);
                    it->win->close();
                    it = _windows.erase(it);
                    continue;
                }
            }

            render_pipeline::render_state rs = {
                .start_sem = acquire_result->acquire_sem,
                .end_sem = acquire_result->render_complete_sem,
                .end_fence = acquire_result->frame_complete_fence,
                .swapchain_image = acquire_result->image,
                .surface = it->render_surface,
                .image_index = acquire_result->image_index,
                .image_width = _rhi_device->get_render_surface_width(it->render_surface),
                .image_height = _rhi_device->get_render_surface_height(it->render_surface),
            };

            auto result = pipeline.render(*this, *_rhi_device, rs);

            if (result == render_pipeline::render_result::REQUEST_RECREATE_SWAPCHAIN || it->framebuffer_resized)
            {
                _rhi_device->recreate_render_surface(it->render_surface,
                                                     {
                                                         .window = it->win,
                                                         .min_image_count = 2,
                                                         .format =
                                                             {
                                                                 .space = rhi::color_space::srgb_nonlinear,
                                                                 .format = rhi::image_format::bgra8_srgb,
                                                             },
                                                         .present_mode = rhi::present_mode::immediate,
                                                         .width = window.framebuffer_width(),
                                                         .height = window.framebuffer_height(),
                                                         .layers = 1,
                                                     });
                it->framebuffer_resized = false;
                continue;
            }
            else if (result == render_pipeline::render_result::FAILURE)
            {
                it->pipeline->destroy(*this, *_rhi_device);
                it = _windows.erase(it);
                continue;
            }

            ++it;
        }

        _rhi_device->end_frame();

        return !_windows.empty();
    }

    void renderer::upload_objects_sync(span<const ecs::archetype_entity> entities, const core::mesh_registry& meshes,
                                       const core::texture_registry& textures, const core::material_registry& materials)
    {
        for (auto&& ctx : _windows)
        {
            ctx.pipeline->upload_objects_sync(*_rhi_device, entities, meshes, textures, materials);
        }
    }

    void render_pipeline::upload_objects_sync([[maybe_unused]] rhi::device& dev,
                                              [[maybe_unused]] span<const ecs::archetype_entity> entities,
                                              [[maybe_unused]] const core::mesh_registry& meshes,
                                              [[maybe_unused]] const core::texture_registry& textures,
                                              [[maybe_unused]] const core::material_registry& materials)
    {
        // Default implementation does nothing.
        // Derived classes can override this method to implement specific upload logic.
    }
} // namespace tempest::graphics