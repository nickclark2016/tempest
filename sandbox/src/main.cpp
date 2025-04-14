#include <tempest/input.hpp>
#include <tempest/logger.hpp>
#include <tempest/rhi.hpp>

namespace rhi = tempest::rhi;

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

    auto commands_complete_fences = tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>>(
        num_frames_in_flight, rhi::typed_rhi_handle<rhi::rhi_handle_type::fence>::null_handle);
    auto image_acquired_sems = tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore>>();
    auto render_complete_sems = tempest::vector<rhi::typed_rhi_handle<rhi::rhi_handle_type::semaphore>>();

    for (uint32_t fif = 0; fif < num_frames_in_flight; ++fif)
    {
        image_acquired_sems.push_back(device.create_semaphore({
            .type = rhi::semaphore_type::BINARY,
            .initial_value = 0,
        }));

        render_complete_sems.push_back(device.create_semaphore({
            .type = rhi::semaphore_type::BINARY,
            .initial_value = 0,
        }));
    }

    uint32_t frame_in_flight = 0;

    while (!window->should_close())
    {
        tempest::core::input::poll();

        if (!commands_complete_fences[frame_in_flight])
        {
            commands_complete_fences[frame_in_flight] = device.create_fence({
                .signaled = false,
            });
        }
        else if (!device.is_signaled(commands_complete_fences[frame_in_flight]))
        {
            // Wait for the fence
            tempest::array fences_to_wait_on = {
                commands_complete_fences[frame_in_flight],
            };
            device.wait(fences_to_wait_on);
        }

        tempest::array fences_to_reset = {
            commands_complete_fences[frame_in_flight],
        };
        device.reset(fences_to_reset);

        device.start_frame();

        auto acquire_result = device.acquire_next_image(render_surface, image_acquired_sems[frame_in_flight]);

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
            .semaphore = image_acquired_sems[frame_in_flight],
            .value = 0,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = render_complete_sems[frame_in_flight],
            .value = 1,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
        });

        default_work_queue.submit(tempest::span(&submit_info, 1), commands_complete_fences[frame_in_flight]);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = render_surface,
            .image_index = acquire_result.value().image_index,
        });
        present_info.wait_semaphores.push_back(render_complete_sems[frame_in_flight]);

        default_work_queue.present(present_info);

        device.end_frame();

        frame_in_flight = (frame_in_flight + 1) % num_frames_in_flight;
    }

    device.destroy_render_surface(render_surface);

    return 0;
}