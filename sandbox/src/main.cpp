#include <tempest/input.hpp>
#include <tempest/rhi.hpp>

namespace rhi = tempest::rhi;

static void recreate_swapchain(rhi::device& device,
                               rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface,
                               rhi::window_surface& window)
{
    device.recreate_render_surface(render_surface, {
                                                       .window = &window,
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
}

int main()
{
    auto instance = rhi::vk::create_instance();
    auto window = rhi::vk::create_window_surface({
        .width = 1920,
        .height = 1080,
        .name = "Sandbox",
        .fullscreen = false,
    });

    auto& device = instance->acquire_device(0);
    auto& default_work_queue = device.get_primary_work_queue();

    auto render_surface = device.create_render_surface({
        .window = window.get(),
        .min_image_count = 2,
        .format =
            {
                .space = rhi::color_space::SRGB_NONLINEAR,
                .format = rhi::image_format::BGRA8_SRGB,
            },
        .present_mode = rhi::present_mode::IMMEDIATE,
        .width = 1920,
        .height = 1080,
        .layers = 1,
    });

    auto num_frames_in_flight = device.frames_in_flight();
    uint32_t frame_in_flight = 0;

    auto color_buffer = device.create_image({
        .format = rhi::image_format::BGRA8_SRGB,
        .type = rhi::image_type::IMAGE_2D,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::SAMPLE_COUNT_1,
        .tiling = rhi::image_tiling_type::OPTIMAL,
        .location = rhi::memory_location::DEVICE,
        .usage = tempest::make_enum_mask(rhi::image_usage::TRANSFER_SRC, rhi::image_usage::TRANSFER_DST,
                                         rhi::image_usage::COLOR_ATTACHMENT),
        .name = "Color Buffer",
    });

    while (!window->should_close())
    {
        tempest::core::input::poll();
        device.start_frame();

        auto acquire_result = device.acquire_next_image(render_surface);

        // Check if the swapchain is out of date or suboptimal
        if (!acquire_result)
        {
            auto error_code = acquire_result.error();
            if (error_code == rhi::swapchain_error_code::OUT_OF_DATE)
            {
                recreate_swapchain(device, render_surface, *window);
                continue;
            }
            else if (error_code == rhi::swapchain_error_code::FAILURE)
            {
                break;
            }
        }

        auto cmds = default_work_queue.get_next_command_list();
        default_work_queue.begin_command_list(cmds, true);

        // Transition the color buffer to TRANSFER_DST layout
        rhi::work_queue::image_barrier color_barrier{
            .image = color_buffer,
            .old_layout = rhi::image_layout::UNDEFINED,
            .new_layout = rhi::image_layout::TRANSFER_DST,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::TOP),
            .src_access = tempest::make_enum_mask(rhi::memory_access::NONE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::CLEAR),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
        };

        default_work_queue.transition_image(cmds, tempest::span(&color_barrier, 1));

        // Clear the color buffer
        default_work_queue.clear_color_image(cmds, color_buffer, rhi::image_layout::TRANSFER_DST, 1.0f, 0.0f, 1.0f,
                                             1.0f);

        // Transition the color buffer to TRANSFER_SRC
        // Transition the swapchain image to TRANSFER_DST layout
        tempest::array color_swap_barriers = {
            rhi::work_queue::image_barrier{
                .image = acquire_result->image,
                .old_layout = rhi::image_layout::UNDEFINED,
                .new_layout = rhi::image_layout::TRANSFER_DST,
                .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
                .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
                .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
                .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            },
            rhi::work_queue::image_barrier{
                .image = color_buffer,
                .old_layout = rhi::image_layout::TRANSFER_DST,
                .new_layout = rhi::image_layout::TRANSFER_SRC,
                .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::CLEAR),
                .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
                .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
                .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_READ),
            },
        };
        default_work_queue.transition_image(cmds, color_swap_barriers);

        // Blit the color buffer to the swapchain image
        default_work_queue.blit(cmds, color_buffer, acquire_result->image);

        // Transition the swapchain image to PRESENT layout
        rhi::work_queue::image_barrier present_barrier{
            .image = acquire_result->image,
            .old_layout = rhi::image_layout::TRANSFER_DST,
            .new_layout = rhi::image_layout::PRESENT,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::BLIT),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::NONE),
        };
        default_work_queue.transition_image(cmds, tempest::span(&present_barrier, 1));

        default_work_queue.end_command_list(cmds);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmds);
        submit_info.wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = acquire_result->acquire_sem,
            .value = 0,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = acquire_result->render_complete_sem,
            .value = 1,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
        });

        default_work_queue.submit(tempest::span(&submit_info, 1), acquire_result->frame_complete_fence);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = render_surface,
            .image_index = acquire_result.value().image_index,
        });
        present_info.wait_semaphores.push_back(acquire_result->render_complete_sem);

        auto present_result = default_work_queue.present(present_info);
        if (present_result == rhi::work_queue::present_result::OUT_OF_DATE ||
            present_result == rhi::work_queue::present_result::SUBOPTIMAL)
        {
            recreate_swapchain(device, render_surface, *window);
        }
        else if (present_result == rhi::work_queue::present_result::ERROR)
        {
            break;
        }

        device.end_frame();

        frame_in_flight = (frame_in_flight + 1) % num_frames_in_flight;
    }

    device.destroy_render_surface(render_surface);

    return 0;
}