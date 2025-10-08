#include <tempest/frame_graph.hpp>

#include <gtest/gtest.h>

TEST(frame_graph, simple_frame_graph)
{
    using namespace tempest;

    auto builder = graphics::graph_builder{};

    auto color_target = builder.create_render_target({
        .format = rhi::image_format::rgba8_srgb,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
        .name = "Color Target",
    });

    auto depth_target = builder.create_render_target({
        .format = rhi::image_format::d32_float,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::depth_attachment),
        .name = "Depth Target",
    });

    builder.create_graphics_pass(
        "Opaque Pass",
        [&](graphics::graphics_task_builder& task) {
            task.write(color_target, rhi::image_layout::color_attachment,
                       make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                       make_enum_mask(rhi::memory_access::color_attachment_write));
            task.write(
                depth_target, rhi::image_layout::depth,
                make_enum_mask(rhi::pipeline_stage::early_fragment_tests, rhi::pipeline_stage::late_fragment_tests),
                make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_graphics_pass(
        "OIT Pass",
        [&](graphics::graphics_task_builder& task) {
            task.read_write(color_target, rhi::image_layout::color_attachment,
                            make_enum_mask(rhi::pipeline_stage::fragment_shader),
                            make_enum_mask(rhi::memory_access::shader_read),
                            make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                            make_enum_mask(rhi::memory_access::color_attachment_write));
            task.read(depth_target, rhi::image_layout::depth_read_only,
                      make_enum_mask(rhi::pipeline_stage::fragment_shader),
                      make_enum_mask(rhi::memory_access::depth_stencil_attachment_read));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    auto queue_cfg = graphics::queue_configuration{
        .graphics_queues = 1,
        .compute_queues = 0,
        .transfer_queues = 0,
    };

    auto plan = tempest::move(builder).compile(queue_cfg);

    ASSERT_EQ(graphics::get_resource_type(color_target), rhi::rhi_handle_type::image);
    ASSERT_EQ(graphics::get_resource_type(depth_target), rhi::rhi_handle_type::image);

    // Check that we have two resources
    ASSERT_EQ(plan.resources.size(), 2);

    // Ensure that we have a single submission
    ASSERT_EQ(plan.submissions.size(), 1);

    auto& submission = plan.submissions[0];
    ASSERT_EQ(submission.type, graphics::work_type::graphics);
    ASSERT_EQ(submission.queue_index, 0);
    ASSERT_EQ(submission.passes.size(), 2);

    // Ensure that the first pass is the opaque pass
    ASSERT_EQ(submission.passes[0].name, "Opaque Pass");
    ASSERT_EQ(submission.passes[0].type, graphics::work_type::graphics);
    ASSERT_EQ(submission.passes[0].accesses.size(), 2); // Write depth, write color

    ASSERT_EQ(submission.passes[1].name, "OIT Pass");
    ASSERT_EQ(submission.passes[1].type, graphics::work_type::graphics);
    ASSERT_EQ(submission.passes[1].accesses.size(), 3); // Read depth, read color, write color

    // There are no cross-queue dependencies, ensure that there are no waits or signals
    ASSERT_EQ(submission.waits.size(), 0);
    ASSERT_EQ(submission.signals.size(), 0);
}

TEST(frame_graph, frame_graph_with_async)
{
    using namespace tempest;

    auto builder = graphics::graph_builder{};

    auto color_target = builder.create_render_target({
        .format = rhi::image_format::rgba8_srgb,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
        .name = "Color Target",
    });

    auto depth_target = builder.create_render_target({
        .format = rhi::image_format::d32_float,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::depth_attachment),
        .name = "Depth Target",
    });

    auto shadow_target = builder.create_render_target({
        .format = rhi::image_format::d32_float,
        .type = rhi::image_type::image_2d,
        .width = 2048,
        .height = 2048,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::depth_attachment, rhi::image_usage::sampled),
        .name = "Shadow Target",
    });

    auto ssao_target = builder.create_render_target({
        .format = rhi::image_format::r8_unorm,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
        .name = "SSAO Target",
    });

    auto ssao_blur_target = builder.create_render_target({
        .format = rhi::image_format::r8_unorm,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
        .name = "SSAO Blur Target",
    });

    auto tonemap_target = builder.create_render_target({
        .format = rhi::image_format::rgba8_srgb,
        .type = rhi::image_type::image_2d,
        .width = 1920,
        .height = 1080,
        .depth = 1,
        .array_layers = 1,
        .mip_levels = 1,
        .sample_count = rhi::image_sample_count::sample_count_1,
        .tiling = rhi::image_tiling_type::optimal,
        .location = rhi::memory_location::device,
        .usage = make_enum_mask(rhi::image_usage::color_attachment, rhi::image_usage::sampled),
        .name = "Tonemap Target",
    });

    // Z pre-pass
    // SSAO Pass (Async compute)
    // SSAO Blur Pass (Async compute)
    // Shadow Pass
    // Opaque Pass
    // OIT Pass
    // Tonemap Pass

    builder.create_graphics_pass(
        "Z Pre-Pass",
        [&](graphics::graphics_task_builder& task) {
            task.write(
                depth_target, rhi::image_layout::depth,
                make_enum_mask(rhi::pipeline_stage::early_fragment_tests, rhi::pipeline_stage::late_fragment_tests),
                make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_graphics_pass(
        "Shadow Pass",
        [&](graphics::graphics_task_builder& task) {
            task.write(
                shadow_target, rhi::image_layout::depth,
                make_enum_mask(rhi::pipeline_stage::early_fragment_tests, rhi::pipeline_stage::late_fragment_tests),
                make_enum_mask(rhi::memory_access::depth_stencil_attachment_write));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_compute_pass(
        "SSAO Pass",
        [&](graphics::compute_task_builder& task) {
            task.prefer_async();
            task.write(ssao_target, rhi::image_layout::general, make_enum_mask(rhi::pipeline_stage::compute_shader),
                       make_enum_mask(rhi::memory_access::shader_write));
            task.read(depth_target, rhi::image_layout::shader_read_only,
                      make_enum_mask(rhi::pipeline_stage::compute_shader),
                      make_enum_mask(rhi::memory_access::shader_read));
        },
        []([[maybe_unused]] graphics::compute_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_compute_pass(
        "SSAO Blur Pass",
        [&](graphics::compute_task_builder& task) {
            task.prefer_async();
            task.write(ssao_blur_target, rhi::image_layout::general,
                       make_enum_mask(rhi::pipeline_stage::compute_shader),
                       make_enum_mask(rhi::memory_access::shader_write));
            task.read(ssao_target, rhi::image_layout::general, make_enum_mask(rhi::pipeline_stage::compute_shader),
                      make_enum_mask(rhi::memory_access::shader_read));
        },
        []([[maybe_unused]] graphics::compute_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_graphics_pass(
        "Opaque Pass",
        [&](graphics::graphics_task_builder& task) {
            task.write(color_target, rhi::image_layout::color_attachment,
                       make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                       make_enum_mask(rhi::memory_access::color_attachment_write));
            task.read(
                depth_target, rhi::image_layout::depth_read_only,
                make_enum_mask(rhi::pipeline_stage::early_fragment_tests, rhi::pipeline_stage::late_fragment_tests),
                make_enum_mask(rhi::memory_access::depth_stencil_attachment_read));
            task.read(shadow_target, rhi::image_layout::shader_read_only,
                      make_enum_mask(rhi::pipeline_stage::fragment_shader),
                      make_enum_mask(rhi::memory_access::shader_read));
            task.read(ssao_blur_target, rhi::image_layout::shader_read_only,
                      make_enum_mask(rhi::pipeline_stage::fragment_shader),
                      make_enum_mask(rhi::memory_access::shader_read));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_graphics_pass(
        "OIT Pass",
        [&](graphics::graphics_task_builder& task) {
            task.read_write(color_target, rhi::image_layout::color_attachment,
                            make_enum_mask(rhi::pipeline_stage::fragment_shader),
                            make_enum_mask(rhi::memory_access::color_attachment_read),
                            make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                            make_enum_mask(rhi::memory_access::color_attachment_write));
            task.read(depth_target, rhi::image_layout::depth_read_only,
                      make_enum_mask(rhi::pipeline_stage::fragment_shader),
                      make_enum_mask(rhi::memory_access::depth_stencil_attachment_read));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    builder.create_graphics_pass(
        "Tonemap Pass",
        [&](graphics::graphics_task_builder& task) {
            task.write(tonemap_target, rhi::image_layout::color_attachment,
                       make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                       make_enum_mask(rhi::memory_access::color_attachment_write));
            task.read(color_target, rhi::image_layout::shader_read_only,
                      make_enum_mask(rhi::pipeline_stage::fragment_shader),
                      make_enum_mask(rhi::memory_access::shader_read));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    const auto queue_cfg = graphics::queue_configuration{
        .graphics_queues = 1,
        .compute_queues = 1,
        .transfer_queues = 0,
    };

    auto plan = tempest::move(builder).compile(queue_cfg);

    ASSERT_EQ(graphics::get_resource_type(color_target), rhi::rhi_handle_type::image);
    ASSERT_EQ(graphics::get_resource_type(depth_target), rhi::rhi_handle_type::image);
    ASSERT_EQ(graphics::get_resource_type(shadow_target), rhi::rhi_handle_type::image);
    ASSERT_EQ(graphics::get_resource_type(ssao_target), rhi::rhi_handle_type::image);
    ASSERT_EQ(graphics::get_resource_type(ssao_blur_target), rhi::rhi_handle_type::image);
    ASSERT_EQ(graphics::get_resource_type(tonemap_target), rhi::rhi_handle_type::image);

    // Check that we have six resources
    ASSERT_EQ(plan.resources.size(), 6);
    ASSERT_EQ(plan.resources[0].creation_info.index(), 2);
    ASSERT_TRUE(plan.resources[0].render_target);
    ASSERT_EQ(plan.resources[1].creation_info.index(), 2);
    ASSERT_TRUE(plan.resources[1].render_target);
    ASSERT_EQ(plan.resources[2].creation_info.index(), 2);
    ASSERT_TRUE(plan.resources[2].render_target);
    ASSERT_EQ(plan.resources[3].creation_info.index(), 2);
    ASSERT_TRUE(plan.resources[3].render_target);
    ASSERT_EQ(plan.resources[4].creation_info.index(), 2);
    ASSERT_TRUE(plan.resources[4].render_target);
    ASSERT_EQ(plan.resources[5].creation_info.index(), 2);
    ASSERT_TRUE(plan.resources[5].render_target);

    // Ensure that we have 4 submissions
    // 1: Z Pre-Pass
    // 2: Compute (SSAO + SSAO Blur)
    // 3: Shadow Pass
    // 4: Opaque + OIT + Tonemap
    // Note that 2 or 3 could be swapped depending on async scheduling

    ASSERT_EQ(plan.submissions.size(), 4);

    // Ensure the first submission is the Z pre-pass
    {
        auto& submission = plan.submissions[0];
        ASSERT_EQ(submission.type, graphics::work_type::graphics);
        ASSERT_EQ(submission.queue_index, 0);
        ASSERT_EQ(submission.passes.size(), 1);
        ASSERT_EQ(submission.passes[0].name, "Z Pre-Pass");
        ASSERT_EQ(submission.passes[0].type, graphics::work_type::graphics);
        ASSERT_EQ(submission.passes[0].accesses.size(), 1); // Write depth
        ASSERT_EQ(submission.waits.size(), 0);
        ASSERT_EQ(submission.signals.size(), 1); // Signal depth written
    }

    auto process_ssao_submission = [](const graphics::submit_instructions& submission) {
        ASSERT_EQ(submission.type, graphics::work_type::compute);
        ASSERT_EQ(submission.queue_index, 0); // Only one compute queue
        ASSERT_EQ(submission.passes.size(), 2);
        ASSERT_EQ(submission.passes[0].name, "SSAO Pass");
        ASSERT_EQ(submission.passes[0].type, graphics::work_type::compute);
        ASSERT_EQ(submission.passes[0].accesses.size(), 2); // Write SSAO, read depth
        ASSERT_EQ(submission.passes[1].name, "SSAO Blur Pass");
        ASSERT_EQ(submission.passes[1].type, graphics::work_type::compute);
        ASSERT_EQ(submission.passes[1].accesses.size(), 2); // Write SSAO Blur, read SSAO

        ASSERT_EQ(submission.waits.size(), 1);   // Wait for depth write
        ASSERT_EQ(submission.signals.size(), 2); // Signal depth read done, SSAO blur written
    };

    auto process_shadow_submission = [](const graphics::submit_instructions& submission) {
        ASSERT_EQ(submission.type, graphics::work_type::graphics);
        ASSERT_EQ(submission.queue_index, 0);
        ASSERT_EQ(submission.passes.size(), 1);
        ASSERT_EQ(submission.passes[0].name, "Shadow Pass");
        ASSERT_EQ(submission.passes[0].type, graphics::work_type::graphics);
        ASSERT_EQ(submission.passes[0].accesses.size(), 1); // Write shadow

        ASSERT_EQ(submission.waits.size(), 0);
        ASSERT_EQ(submission.signals.size(), 0);
    };

    // Ensure the second submission is either SSAO or Shadow. Ensure the third is the other.
    {
        auto& submission_1 = plan.submissions[1];
        auto& submission_2 = plan.submissions[2];

        if (submission_1.type == graphics::work_type::compute)
        {
            process_ssao_submission(submission_1);
            process_shadow_submission(submission_2);
        }
        else
        {
            process_shadow_submission(submission_1);
            process_ssao_submission(submission_2);
        }
    }

    // Ensure the fourth submission is the opaque + oit + tonemap
    {
        auto& submission = plan.submissions[3];
        ASSERT_EQ(submission.type, graphics::work_type::graphics);
        ASSERT_EQ(submission.queue_index, 0);
        ASSERT_EQ(submission.passes.size(), 3);
        ASSERT_EQ(submission.passes[0].name, "Opaque Pass");
        ASSERT_EQ(submission.passes[0].type, graphics::work_type::graphics);
        ASSERT_EQ(submission.passes[0].accesses.size(), 4); // Write color, read depth, read shadow, read ssao
        ASSERT_EQ(submission.passes[1].name, "OIT Pass");
        ASSERT_EQ(submission.passes[1].type, graphics::work_type::graphics);
        ASSERT_EQ(submission.passes[1].accesses.size(), 3); // Read color, write color, read depth
        ASSERT_EQ(submission.passes[2].name, "Tonemap Pass");
        ASSERT_EQ(submission.passes[2].type, graphics::work_type::graphics);
        ASSERT_EQ(submission.passes[2].accesses.size(), 2); // Write tonemap, read color
        ASSERT_EQ(submission.waits.size(), 2);              // Wait for depth read done, ssao blur written
        ASSERT_EQ(submission.signals.size(), 0);
    }
}

TEST(frame_graph, imported_swapchain)
{
    using namespace tempest;

    auto builder = graphics::graph_builder{};
    auto render_surface_handle = rhi::typed_rhi_handle<rhi::rhi_handle_type::render_surface>{.id = 1, .generation = 0};

    auto imported_surface = builder.import_render_surface("Main Window Surface", render_surface_handle);

    builder.create_graphics_pass(
        "Present Pass",
        [&](graphics::graphics_task_builder& task) {
            task.write(imported_surface, rhi::image_layout::color_attachment,
                       make_enum_mask(rhi::pipeline_stage::color_attachment_output),
                       make_enum_mask(rhi::memory_access::color_attachment_write));
        },
        []([[maybe_unused]] graphics::graphics_task_execution_context& ctx) {
            // Record commands here
        });

    auto queue_cfg = graphics::queue_configuration{
        .graphics_queues = 1,
        .compute_queues = 0,
        .transfer_queues = 0,
    };

    auto plan = tempest::move(builder).compile(queue_cfg);

    ASSERT_EQ(graphics::get_resource_type(imported_surface), rhi::rhi_handle_type::render_surface);

    ASSERT_EQ(plan.resources.size(), 1);
    ASSERT_EQ(plan.resources[0].creation_info.index(), 0); // monostate, as it's imported
    ASSERT_EQ(plan.resources[0].per_frame, false);
    ASSERT_EQ(plan.resources[0].temporal, false);
    ASSERT_EQ(plan.resources[0].render_target, true);
    ASSERT_EQ(plan.resources[0].presentable, true);

    ASSERT_EQ(plan.submissions.size(), 1);
    ASSERT_EQ(plan.submissions[0].passes.size(), 1);
    ASSERT_EQ(plan.submissions[0].passes[0].name, "Present Pass");
}
