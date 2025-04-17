#include <tempest/input.hpp>
#include <tempest/rhi.hpp>

namespace rhi = tempest::rhi;

static void recreate_swapchain(rhi::device& device, rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface> render_surface,
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
    auto render_complete_sems = tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore>>();

    for (uint32_t i = 0; i < num_frames_in_flight; ++i)
    {
        render_complete_sems.push_back(device.create_semaphore({}));
    }

    uint32_t frame_in_flight = 0;

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

        auto cmds = default_work_queue.get_next_command_list(frame_in_flight);
        default_work_queue.begin_command_list(cmds, true);

        rhi::work_queue::image_barrier undefined_color_barrier = {
            .image = acquire_result->image,
            .old_layout = rhi::image_layout::UNDEFINED,
            .new_layout = rhi::image_layout::TRANSFER_DST,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::CLEAR, rhi::pipeline_stage::ALL_TRANSFER),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
        };

        default_work_queue.transition_image(cmds, tempest::span(&undefined_color_barrier, 1));
        default_work_queue.clear_color_image(cmds, acquire_result->image, rhi::image_layout::TRANSFER_DST, 1.0f, 1.0f,
                                             0.0f, 1.0f);

        rhi::work_queue::image_barrier color_present_barrier = {
            .image = acquire_result->image,
            .old_layout = rhi::image_layout::TRANSFER_DST,
            .new_layout = rhi::image_layout::PRESENT,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::CLEAR, rhi::pipeline_stage::ALL_TRANSFER),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::NONE),
        };

        default_work_queue.transition_image(cmds, tempest::span(&color_present_barrier, 1));

        default_work_queue.end_command_list(cmds);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmds);
        submit_info.wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = acquire_result->acquire_sem,
            .value = 0,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = render_complete_sems[frame_in_flight],
            .value = 1,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
        });

        default_work_queue.submit(tempest::span(&submit_info, 1), acquire_result->frame_complete_fence);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = render_surface,
            .image_index = acquire_result.value().image_index,
        });
        present_info.wait_semaphores.push_back(render_complete_sems[frame_in_flight]);

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