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

    void renderer::register_window(rhi::window_surface* window, unique_ptr<render_pipeline> pipeline)
    {
        auto render_surface = _rhi_device->create_render_surface({
            .window = window,
            .min_image_count = 2,
            .format =
                {
                    .space = rhi::color_space::SRGB_NONLINEAR,
                    .format = rhi::image_format::BGRA8_SRGB,
                },
            .present_mode = rhi::present_mode::IMMEDIATE,
            .width = window->width(),
            .height = window->height(),
            .layers = 1,
        });

        _windows.push_back({
            .win = window,
            .render_surface = render_surface,
            .pipeline = move(pipeline),
        });

        _windows.back().pipeline->initialize(*this, *_rhi_device);

        // On window close, unregister the window
        window->register_close_callback([this, window]() { unregister_window(window); });
    }

    void renderer::unregister_window(rhi::window_surface* window)
    {
        tempest::erase_if(_windows, [window](const auto& payload) { return payload.win == window; });
    }

    bool renderer::render()
    {
        _rhi_device->start_frame();

        for (auto it = _windows.begin(); it != _windows.end();)
        {
            auto& window = *it->win;

            if (window.should_close())
            {
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
                if (error_code == rhi::swapchain_error_code::OUT_OF_DATE)
                {
                    _rhi_device->recreate_render_surface(it->render_surface,
                                                         {
                                                             .window = it->win,
                                                             .min_image_count = 2,
                                                             .format =
                                                                 {
                                                                     .space = rhi::color_space::SRGB_NONLINEAR,
                                                                     .format = rhi::image_format::BGRA8_SRGB,
                                                                 },
                                                             .present_mode = rhi::present_mode::IMMEDIATE,
                                                             .width = window.width(),
                                                             .height = window.height(),
                                                             .layers = 1,
                                                         });
                    continue;
                }
                else if (error_code == rhi::swapchain_error_code::FAILURE)
                {
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
            };

            auto result = pipeline.render(*this, *_rhi_device, rs);

            if (result == render_pipeline::render_result::REQUEST_RECREATE_SWAPCHAIN)
            {
                _rhi_device->recreate_render_surface(it->render_surface,
                                                     {
                                                         .window = it->win,
                                                         .min_image_count = 2,
                                                         .format =
                                                             {
                                                                 .space = rhi::color_space::SRGB_NONLINEAR,
                                                                 .format = rhi::image_format::BGRA8_SRGB,
                                                             },
                                                         .present_mode = rhi::present_mode::IMMEDIATE,
                                                         .width = window.width(),
                                                         .height = window.height(),
                                                         .layers = 1,
                                                     });
                continue;
            }
            else if (result == render_pipeline::render_result::FAILURE)
            {
                it = _windows.erase(it);
                continue;
            }

            ++it;
        }

        _rhi_device->end_frame();

        return !_windows.empty();
    }
} // namespace tempest::graphics