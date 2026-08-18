#include <gtest/gtest.h>

#include <tempest/render_graph/render_graph.hpp>

namespace tempest::render_graph
{
    TEST(render_graph_test, basic_graphics_and_compute_pipeline)
    {
        auto rg = render_graph{1920, 1080};

        struct gbuffer_data
        {
            rg_texture_id albedo;
            rg_texture_id depth;
        };

        // GBuffer pass
        const auto& gbuffer = rg.add_graphics_pass<gbuffer_data>(
            "GBufferPass",
            [](pass_builder& builder, gbuffer_data& data) {
                const auto albedo_initial = builder.create_texture(rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "Albedo",
                });

                const auto depth_initial = builder.create_texture(rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::depth32_float,
                    .name = "Depth",
                });

                data.albedo = builder.set_color_attachment(
                    0,
                    rg_color_attachment{
                        .texture = albedo_initial,
                        .load_op = rhi::load_op::clear,
                        .store_op = rhi::store_op::store,
                        .clear_value = {0.1F, 0.2F, 0.3F, 1.0F},
                    });

                data.depth = builder.set_depth_stencil_attachment(rg_depth_stencil_attachment{
                    .texture = depth_initial,
                    .depth_load_op = rhi::load_op::clear,
                    .depth_store_op = rhi::store_op::store,
                    .clear_value = {1.0F, 0},
                });
            },
            []([[maybe_unused]] const gbuffer_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        EXPECT_EQ(gbuffer.albedo.version, 1U);
        EXPECT_EQ(gbuffer.depth.version, 1U);

        struct lighting_data
        {
            rg_texture_id albedo_in;
            rg_texture_id depth_in;
            rg_texture_id lit_hdr;
        };

        // Lighting compute pass
        const auto& lighting = rg.add_compute_pass<lighting_data>(
            "LightingPass",
            [&gbuffer](pass_builder& builder, lighting_data& data) {
                data.albedo_in = builder.read(gbuffer.albedo, rhi::pipeline_stage::compute,
                                              rhi::resource_access::read,
                                              rhi::image_layout::general);
                data.depth_in = builder.read(gbuffer.depth, rhi::pipeline_stage::compute,
                                             rhi::resource_access::read,
                                             rhi::image_layout::general);

                const auto lit_initial = builder.create_texture(rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba16_float,
                    .name = "LitHDR",
                });

                data.lit_hdr = builder.write(lit_initial, rhi::pipeline_stage::compute,
                                             rhi::resource_access::write,
                                             rhi::image_layout::general);
            },
            [](const lighting_data&, pass_execution_context&, rhi::command_list&) {});

        EXPECT_EQ(lighting.albedo_in.version, 1U);
        EXPECT_EQ(lighting.lit_hdr.version, 1U);

        // Dummy swapchain handle
        const auto dummy_swapchain = rhi::texture_handle{.handle = 100};
        rg.add_present_pass(lighting.lit_hdr, dummy_swapchain);

        const auto result = rg.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        ASSERT_EQ(dag.sorted_pass_indices.size(), 3U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U); // GBufferPass
        EXPECT_EQ(dag.sorted_pass_indices[1], 1U); // LightingPass
        EXPECT_EQ(dag.sorted_pass_indices[2], 2U); // PresentPass

        // Verify dynamic sizing resolution
        const auto resolved = rg.resolve_texture_size(gbuffer.albedo);
        EXPECT_EQ(resolved.width, 1920U);
        EXPECT_EQ(resolved.height, 1080U);
    }

    TEST(render_graph_test, passthrough_and_fallback_builder)
    {
        auto rg = render_graph{1920, 1080};

        struct source_data
        {
            rg_texture_id tex;
        };

        const auto& src = rg.add_graphics_pass<source_data>(
            "SourcePass",
            [](pass_builder& builder, source_data& data) {
                const auto t0 = builder.create_texture(rg_texture_desc{.name = "MainColor"});
                data.tex = builder.write(t0, rhi::pipeline_stage::attachment_output,
                                         rhi::resource_access::write,
                                         rhi::image_layout::general);
            },
            [](const source_data&, pass_execution_context&, rhi::command_list&) {});

        struct postprocess_data
        {
            rg_texture_id output;
        };

        // Disabled postprocess pass using passthrough helper
        const auto& post = rg.add_graphics_pass<postprocess_data>(
            "PostProcessPass",
            [&src](pass_builder& builder, postprocess_data& data) {
                data.output = builder.passthrough(src.tex);
                builder.set_enable_condition([] { return false; });
            },
            [](const postprocess_data&, pass_execution_context&, rhi::command_list&) {});

        struct consumer_data
        {
            rg_texture_id in_tex;
        };

        rg.add_graphics_pass<consumer_data>(
            "ConsumerPass",
            [&post](pass_builder& builder, consumer_data& data) {
                data.in_tex = builder.read(post.output, rhi::pipeline_stage::fragment,
                                           rhi::resource_access::read,
                                           rhi::image_layout::general);
                builder.mark_sink();
            },
            [](const consumer_data&, pass_execution_context&, rhi::command_list&) {});

        const auto result = rg.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        // PostProcessPass should be skipped, so only 2 passes execute
        ASSERT_EQ(dag.sorted_pass_indices.size(), 2U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U); // SourcePass
        EXPECT_EQ(dag.sorted_pass_indices[1], 2U); // ConsumerPass

        // Verify that post.output resolves to src.tex
        ASSERT_TRUE(dag.resolved_texture_aliases.contains(post.output));
        EXPECT_EQ(dag.resolved_texture_aliases.find(post.output)->second, src.tex);
    }

    TEST(render_graph_test, buffer_pipeline_and_transfer)
    {
        auto rg = render_graph{1920, 1080};

        struct compute_buf_data
        {
            rg_buffer_id buf;
        };

        const auto& compute_pass = rg.add_compute_pass<compute_buf_data>(
            "BufferComputePass",
            [](pass_builder& builder, compute_buf_data& data) {
                const auto b0 = builder.create_buffer(rg_buffer_desc{
                    .size = 4096,
                    .name = "SSBO",
                });
                data.buf = builder.write(b0, rhi::pipeline_stage::compute, rhi::resource_access::write);
            },
            [](const compute_buf_data&, pass_execution_context&, rhi::command_list&) {});

        struct readback_data
        {
            rg_buffer_id read_buf;
        };

        rg.add_transfer_pass<readback_data>(
            "ReadbackPass",
            [&compute_pass](pass_builder& builder, readback_data& data) {
                data.read_buf = builder.read(compute_pass.buf, rhi::pipeline_stage::copy, rhi::resource_access::read);
                builder.mark_sink();
            },
            [](const readback_data&, pass_execution_context&, rhi::command_list&) {});

        const auto result = rg.compile();
        ASSERT_TRUE(result.has_value());

        const auto& dag = result.value();
        ASSERT_EQ(dag.sorted_pass_indices.size(), 2U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U);
        EXPECT_EQ(dag.sorted_pass_indices[1], 1U);

        ASSERT_TRUE(dag.buffer_lifetimes.contains(compute_pass.buf.id));
        EXPECT_EQ(dag.buffer_lifetimes.find(compute_pass.buf.id)->second.first_pass, 0U);
        EXPECT_EQ(dag.buffer_lifetimes.find(compute_pass.buf.id)->second.last_pass, 1U);
    }

    TEST(render_graph_test, surface_resize_evaluation)
    {
        auto rg = render_graph{1280, 720};

        const auto full_res = rg.create_texture(rg_texture_desc{
            .size = rg_texture_size::surface_relative(1.0F, 1.0F),
            .name = "FullRes",
        });

        const auto half_res = rg.create_texture(rg_texture_desc{
            .size = rg_texture_size::surface_relative(0.5F, 0.5F),
            .name = "HalfRes",
        });

        const auto fixed_res = rg.create_texture(rg_texture_desc{
            .size = rg_texture_size::absolute(256, 256),
            .name = "FixedRes",
        });

        auto full_eval = rg.resolve_texture_size(full_res);
        auto half_eval = rg.resolve_texture_size(half_res);
        auto fixed_eval = rg.resolve_texture_size(fixed_res);

        EXPECT_EQ(full_eval.width, 1280U);
        EXPECT_EQ(full_eval.height, 720U);
        EXPECT_EQ(half_eval.width, 640U);
        EXPECT_EQ(half_eval.height, 360U);
        EXPECT_EQ(fixed_eval.width, 256U);
        EXPECT_EQ(fixed_eval.height, 256U);

        // Dynamically resize surface to 4K (3840x2160)
        rg.set_surface_size(3840, 2160);

        full_eval = rg.resolve_texture_size(full_res);
        half_eval = rg.resolve_texture_size(half_res);
        fixed_eval = rg.resolve_texture_size(fixed_res);

        EXPECT_EQ(full_eval.width, 3840U);
        EXPECT_EQ(full_eval.height, 2160U);
        EXPECT_EQ(half_eval.width, 1920U);
        EXPECT_EQ(half_eval.height, 1080U);
        EXPECT_EQ(fixed_eval.width, 256U);
        EXPECT_EQ(fixed_eval.height, 256U);
    }
} // namespace tempest::render_graph
