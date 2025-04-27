#include <tempest/input.hpp>
#include <tempest/pipelines/pbr_pipeline.hpp>
#include <tempest/render_pipeline.hpp>
#include <tempest/rhi.hpp>
#include <tempest/tuple.hpp>

namespace rhi = tempest::rhi;

struct clear_render_pipeline : tempest::graphics::render_pipeline
{
    float r;
    float g;
    float b;
    float a;

    clear_render_pipeline(float r, float g, float b, float a) : r(r), g(g), b(b), a(a)
    {
    }

    void initialize([[maybe_unused]] tempest::graphics::renderer& parent, [[maybe_unused]] rhi::device& dev) override
    {
    }

    render_result render([[maybe_unused]] tempest::graphics::renderer& parent, rhi::device& dev,
                         const render_state& rs) const override
    {
        auto& work_queue = dev.get_primary_work_queue();
        auto cmds = work_queue.get_next_command_list();
        work_queue.begin_command_list(cmds, true);

        rhi::work_queue::image_barrier to_tx_dst = {
            .image = rs.swapchain_image,
            .old_layout = rhi::image_layout::UNDEFINED,
            .new_layout = rhi::image_layout::TRANSFER_DST,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::CLEAR),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
        };

        work_queue.transition_image(cmds, tempest::span(&to_tx_dst, 1));
        work_queue.clear_color_image(cmds, rs.swapchain_image, rhi::image_layout::TRANSFER_DST, r, g, b, a);

        rhi::work_queue::image_barrier to_present = {
            .image = rs.swapchain_image,
            .old_layout = rhi::image_layout::TRANSFER_DST,
            .new_layout = rhi::image_layout::PRESENT,
            .src_stages = tempest::make_enum_mask(rhi::pipeline_stage::CLEAR),
            .src_access = tempest::make_enum_mask(rhi::memory_access::TRANSFER_WRITE),
            .dst_stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
            .dst_access = tempest::make_enum_mask(rhi::memory_access::NONE),
        };

        work_queue.transition_image(cmds, tempest::span(&to_present, 1));

        work_queue.end_command_list(cmds);

        rhi::work_queue::submit_info submit_info;
        submit_info.command_lists.push_back(cmds);
        submit_info.wait_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.start_sem,
            .value = 0,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::ALL_TRANSFER),
        });
        submit_info.signal_semaphores.push_back(rhi::work_queue::semaphore_submit_info{
            .semaphore = rs.end_sem,
            .value = 1,
            .stages = tempest::make_enum_mask(rhi::pipeline_stage::BOTTOM),
        });

        work_queue.submit(tempest::span(&submit_info, 1), rs.end_fence);

        rhi::work_queue::present_info present_info;
        present_info.swapchain_images.push_back(rhi::work_queue::swapchain_image_present_info{
            .render_surface = rs.surface,
            .image_index = rs.image_index,
        });
        present_info.wait_semaphores.push_back(rs.end_sem);

        auto present_result = work_queue.present(present_info);
        if (present_result == rhi::work_queue::present_result::OUT_OF_DATE ||
            present_result == rhi::work_queue::present_result::SUBOPTIMAL)
        {
            return render_result::REQUEST_RECREATE_SWAPCHAIN;
        }
        else if (present_result == rhi::work_queue::present_result::ERROR)
        {
            return render_result::FAILURE;
        }
        return render_result::SUCCESS;
    }

    void destroy([[maybe_unused]] tempest::graphics::renderer&, [[maybe_unused]] rhi::device&) override
    {
    }
};

int main()
{
    auto renderer = tempest::graphics::renderer();

    auto win1 = renderer.create_window({
        .width = 1920,
        .height = 1080,
        .name = "Window 1",
        .fullscreen = false,
    });

    auto win2 = renderer.create_window({
        .width = 1920,
        .height = 1080,
        .name = "Window 2",
        .fullscreen = false,
    });

    auto win3 = renderer.create_window({
        .width = 1920,
        .height = 1080,
        .name = "Window 3",
        .fullscreen = false,
    });

    renderer.register_window(win1.get(), tempest::make_unique<clear_render_pipeline>(1.0f, 0.0f, 0.0f, 1.0f));
    renderer.register_window(win2.get(), tempest::make_unique<clear_render_pipeline>(0.0f, 1.0f, 0.0f, 1.0f));
    renderer.register_window(win3.get(),
                             tempest::make_unique<tempest::graphics::pbr_pipeline>(win3->width(), win3->height()));

    while (true)
    {
        tempest::core::input::poll();

        bool windows_open = renderer.render();
        if (!windows_open)
        {
            break;
        }
    }

    return 0;
}