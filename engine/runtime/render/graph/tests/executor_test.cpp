#include <gtest/gtest.h>

#include <tempest/render_graph/render_graph.hpp>

namespace tempest::render_graph
{
    namespace
    {
        class mock_cmd_list final : public rhi::command_list
        {
          public:
            uint32_t begin_calls = 0;
            uint32_t end_calls = 0;
            uint32_t begin_render_pass_calls = 0;
            uint32_t end_render_pass_calls = 0;
            uint32_t pipeline_barrier_calls = 0;

            auto begin() const -> void override
            {
                const_cast<mock_cmd_list*>(this)->begin_calls++;
            }

            auto end() const -> void override
            {
                const_cast<mock_cmd_list*>(this)->end_calls++;
            }

            auto pipeline_barrier([[maybe_unused]] span<const rhi::texture_barrier> texture_barriers,
                                  [[maybe_unused]] span<const rhi::buffer_barrier> buffer_barriers) const -> void override
            {
                const_cast<mock_cmd_list*>(this)->pipeline_barrier_calls++;
            }

            auto signal_event([[maybe_unused]] rhi::event_handle event,
                              [[maybe_unused]] span<const rhi::texture_barrier> texture_sources,
                              [[maybe_unused]] span<const rhi::buffer_barrier> buffer_sources) const -> void override
            {
            }

            auto wait_event([[maybe_unused]] rhi::event_handle event,
                            [[maybe_unused]] span<const rhi::texture_barrier> texture_destinations,
                            [[maybe_unused]] span<const rhi::buffer_barrier> buffer_destinations) const -> void override
            {
            }

            auto reset_event([[maybe_unused]] rhi::event_handle event,
                             [[maybe_unused]] enum_mask<rhi::pipeline_stage> stages) const -> void override
            {
            }

            auto push_constants([[maybe_unused]] enum_mask<rhi::shader_stage> stages,
                                [[maybe_unused]] uint32_t offset,
                                [[maybe_unused]] span<const byte> data) -> void override
            {
            }

            auto begin_render_pass([[maybe_unused]] span<const rhi::color_attachment> color_attachments,
                                   [[maybe_unused]] optional<rhi::depth_stencil_attachment> depth_stencil_attachment,
                                   [[maybe_unused]] uint32_t width,
                                   [[maybe_unused]] uint32_t height) -> void override
            {
                begin_render_pass_calls++;
            }

            auto end_render_pass() -> void override
            {
                end_render_pass_calls++;
            }

            auto bind_pipeline([[maybe_unused]] rhi::graphics_pipeline_handle pipeline) -> void override {}
            auto set_viewport([[maybe_unused]] float x, [[maybe_unused]] float y,
                              [[maybe_unused]] float width, [[maybe_unused]] float height,
                              [[maybe_unused]] float min_depth, [[maybe_unused]] float max_depth) -> void override
            {
            }
            auto set_scissor([[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                             [[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) -> void override
            {
            }
            auto set_depth_bias([[maybe_unused]] float constant_factor, [[maybe_unused]] float clamp,
                                [[maybe_unused]] float slope_factor) -> void override
            {
            }
            auto set_stencil_reference([[maybe_unused]] uint32_t reference) -> void override {}
            auto set_stencil_compare_mask([[maybe_unused]] uint32_t compare_mask) -> void override {}
            auto set_stencil_write_mask([[maybe_unused]] uint32_t write_mask) -> void override {}
            auto bind_index_buffer([[maybe_unused]] rhi::buffer_handle buffer,
                                   [[maybe_unused]] rhi::index_type type,
                                   [[maybe_unused]] uint64_t offset) -> void override
            {
            }
            auto draw([[maybe_unused]] uint32_t vertex_count, [[maybe_unused]] uint32_t instance_count,
                      [[maybe_unused]] uint32_t first_vertex, [[maybe_unused]] uint32_t first_instance) -> void override
            {
            }
            auto draw_indexed([[maybe_unused]] uint32_t index_count, [[maybe_unused]] uint32_t instance_count,
                              [[maybe_unused]] uint32_t first_index, [[maybe_unused]] int32_t vertex_offset,
                              [[maybe_unused]] uint32_t first_instance) -> void override
            {
            }
            auto draw_indirect([[maybe_unused]] rhi::buffer_handle buffer, [[maybe_unused]] uint64_t offset,
                               [[maybe_unused]] uint32_t draw_count, [[maybe_unused]] uint32_t stride) -> void override
            {
            }
            auto draw_indexed_indirect([[maybe_unused]] rhi::buffer_handle buffer, [[maybe_unused]] uint64_t offset,
                                       [[maybe_unused]] uint32_t draw_count, [[maybe_unused]] uint32_t stride) -> void override
            {
            }
            auto draw_indirect_count([[maybe_unused]] rhi::buffer_handle buffer, [[maybe_unused]] uint64_t offset,
                                     [[maybe_unused]] rhi::buffer_handle count_buffer,
                                     [[maybe_unused]] uint64_t count_buffer_offset,
                                     [[maybe_unused]] uint32_t max_draw_count,
                                     [[maybe_unused]] uint32_t stride) -> void override
            {
            }
            auto draw_indexed_indirect_count([[maybe_unused]] rhi::buffer_handle buffer,
                                             [[maybe_unused]] uint64_t offset,
                                             [[maybe_unused]] rhi::buffer_handle count_buffer,
                                             [[maybe_unused]] uint64_t count_buffer_offset,
                                             [[maybe_unused]] uint32_t max_draw_count,
                                             [[maybe_unused]] uint32_t stride) -> void override
            {
            }

            auto bind_pipeline([[maybe_unused]] rhi::compute_pipeline_handle pipeline) -> void override {}
            auto dispatch([[maybe_unused]] uint32_t group_count_x, [[maybe_unused]] uint32_t group_count_y,
                          [[maybe_unused]] uint32_t group_count_z) -> void override
            {
            }
            auto dispatch_indirect([[maybe_unused]] rhi::buffer_handle buffer,
                                   [[maybe_unused]] uint64_t offset) -> void override
            {
            }

            auto copy_buffer([[maybe_unused]] rhi::buffer_handle src, [[maybe_unused]] rhi::buffer_handle dst,
                             [[maybe_unused]] span<const rhi::buffer_copy_region> regions) -> void override
            {
            }
            auto copy_buffer_to_texture([[maybe_unused]] rhi::buffer_handle src,
                                        [[maybe_unused]] rhi::texture_handle dst,
                                        [[maybe_unused]] span<const rhi::buffer_texture_copy_region> regions)
                -> void override
            {
            }
            auto copy_texture_to_buffer([[maybe_unused]] rhi::texture_handle src,
                                        [[maybe_unused]] rhi::buffer_handle dst,
                                        [[maybe_unused]] span<const rhi::buffer_texture_copy_region> regions)
                -> void override
            {
            }
            auto blit_texture([[maybe_unused]] rhi::texture_handle src,
                              [[maybe_unused]] rhi::texture_handle dst,
                              [[maybe_unused]] span<const rhi::texture_blit_region> regions,
                              [[maybe_unused]] rhi::filter_mode filter) -> void override
            {
            }

            vector<string> debug_regions;
            vector<string> debug_markers;
            uint32_t begin_debug_region_calls = 0;
            uint32_t end_debug_region_calls = 0;
            uint32_t insert_debug_marker_calls = 0;

            auto begin_debug_region(const rhi::debug_label& label) -> void override
            {
                begin_debug_region_calls++;
                debug_regions.push_back(string{label.name.data(), label.name.size()});
            }

            auto end_debug_region() -> void override
            {
                end_debug_region_calls++;
            }

            auto insert_debug_marker(const rhi::debug_label& label) -> void override
            {
                insert_debug_marker_calls++;
                debug_markers.push_back(string{label.name.data(), label.name.size()});
            }
        };

        class mock_execution_port final : public rhi::execution_port
        {
          public:
            mock_cmd_list cmd;
            uint32_t submit_calls = 0;
            vector<rhi::device_sync_point> last_wait_sync;
            vector<rhi::device_sync_point> last_signal_sync;
            vector<string> debug_regions;
            vector<string> debug_markers;
            uint32_t begin_debug_region_calls = 0;
            uint32_t end_debug_region_calls = 0;
            uint32_t insert_debug_marker_calls = 0;

            auto wait_idle() -> void override {}

            auto begin_debug_region(const rhi::debug_label& label) -> void override
            {
                begin_debug_region_calls++;
                debug_regions.push_back(string{label.name.data(), label.name.size()});
            }

            auto end_debug_region() -> void override
            {
                end_debug_region_calls++;
            }

            auto insert_debug_marker(const rhi::debug_label& label) -> void override
            {
                insert_debug_marker_calls++;
                debug_markers.push_back(string{label.name.data(), label.name.size()});
            }

            [[nodiscard]] auto acquire_command_list(
                [[maybe_unused]] uint32_t thread_id = 0,
                [[maybe_unused]] rhi::command_list_lifetime lifetime = rhi::command_list_lifetime::transient)
                -> rhi::command_list& override
            {
                return cmd;
            }

            [[nodiscard]] auto submit([[maybe_unused]] span<const rhi::command_list*> commands,
                                      span<const rhi::device_sync_point> wait_semaphores,
                                      span<const rhi::device_sync_point> signal_semaphores)
                -> expected<void, rhi::submit_error> override
            {
                submit_calls++;
                last_wait_sync.clear();
                for (const auto& w : wait_semaphores)
                {
                    last_wait_sync.push_back(w);
                }
                last_signal_sync.clear();
                for (const auto& s : signal_semaphores)
                {
                    last_signal_sync.push_back(s);
                }
                return {};
            }

            [[nodiscard]] auto get_timeline_sync_point() const noexcept -> rhi::host_sync_point override { return {}; }
        };

        class mock_device_with_ports final : public rhi::device
        {
          public:
            mock_execution_port graphics_port;
            mock_execution_port compute_port;
            mock_execution_port transfer_port;

            uint64_t next_h = 1;
            uint32_t next_desc = 1;

            auto wait_idle() -> void override {}
            auto wait_for_sync([[maybe_unused]] rhi::host_sync_point sync_point) -> void override {}

            [[nodiscard]] auto is_ray_tracing_supported() const -> bool override { return false; }
            [[nodiscard]] auto is_mesh_shading_supported() const -> bool override { return false; }
            [[nodiscard]] auto is_ray_query_supported() const -> bool override { return false; }

            [[nodiscard]] auto create_raw_surface([[maybe_unused]] rhi::native_wsi_handle native_window_handle)
                -> expected<rhi::raw_surface_handle, rhi::raw_surface_creation_error> override
            {
                return rhi::raw_surface_handle{.handle = next_h++};
            }

            [[nodiscard]] auto get_surface_capabilities([[maybe_unused]] rhi::raw_surface_handle surface)
                -> rhi::surface_capabilities override
            {
                return {};
            }

            [[nodiscard]] auto create_render_surface([[maybe_unused]] const rhi::render_surface_desc& desc)
                -> unique_ptr<rhi::render_surface> override
            {
                return nullptr;
            }

            auto destroy_render_surface([[maybe_unused]] unique_ptr<rhi::render_surface> surface) -> void override {}
            auto destroy_raw_surface([[maybe_unused]] rhi::raw_surface_handle surface) -> void override {}

            [[nodiscard]] auto get_semaphore_value([[maybe_unused]] rhi::semaphore_handle semaphore) const -> uint64_t override { return 0; }

            [[nodiscard]] auto get_graphics_execution_port() -> rhi::execution_port& override
            {
                return graphics_port;
            }

            [[nodiscard]] auto get_async_compute_execution_port() -> rhi::execution_port& override
            {
                return compute_port;
            }

            [[nodiscard]] auto get_async_transfer_execution_port() -> rhi::execution_port& override
            {
                return transfer_port;
            }

            [[nodiscard]] auto create_buffer([[maybe_unused]] const rhi::buffer_desc& desc) -> rhi::buffer_handle override
            {
                return rhi::buffer_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_texture([[maybe_unused]] const rhi::texture_desc& desc) -> rhi::texture_handle override
            {
                return rhi::texture_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_texture_view([[maybe_unused]] rhi::texture_handle texture,
                                                   [[maybe_unused]] const rhi::texture_view_desc& desc)
                -> rhi::texture_view_handle override
            {
                return rhi::texture_view_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_sampler([[maybe_unused]] const rhi::sampler_desc& desc) -> rhi::sampler_handle override
            {
                return rhi::sampler_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_graphics_pipeline([[maybe_unused]] const rhi::graphics_pipeline_desc& desc)
                -> rhi::graphics_pipeline_handle override
            {
                return rhi::graphics_pipeline_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_compute_pipeline([[maybe_unused]] const rhi::compute_pipeline_desc& desc)
                -> rhi::compute_pipeline_handle override
            {
                return rhi::compute_pipeline_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_event() -> rhi::event_handle override
            {
                return rhi::event_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_timeline_semaphore() -> rhi::semaphore_handle override
            {
                return rhi::semaphore_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_binary_semaphore() -> rhi::semaphore_handle override
            {
                return rhi::semaphore_handle{.handle = next_h++};
            }

            auto destroy_buffer([[maybe_unused]] rhi::buffer_handle buffer) -> void override {}
            auto destroy_texture([[maybe_unused]] rhi::texture_handle texture) -> void override {}
            auto destroy_texture_view([[maybe_unused]] rhi::texture_view_handle view) -> void override {}
            auto destroy_sampler([[maybe_unused]] rhi::sampler_handle sampler) -> void override {}
            auto destroy_graphics_pipeline([[maybe_unused]] rhi::graphics_pipeline_handle pipeline) -> void override {}
            auto destroy_compute_pipeline([[maybe_unused]] rhi::compute_pipeline_handle pipeline) -> void override {}
            auto destroy_event([[maybe_unused]] rhi::event_handle event) -> void override {}
            auto destroy_semaphore([[maybe_unused]] rhi::semaphore_handle semaphore) -> void override {}

            [[nodiscard]] auto allocate_descriptor([[maybe_unused]] rhi::descriptor_type type) -> rhi::descriptor_handle override
            {
                return rhi::descriptor_handle{.index = next_desc++, .generation = 1};
            }

            auto free_descriptor([[maybe_unused]] rhi::descriptor_type type,
                                 [[maybe_unused]] rhi::descriptor_handle descriptor) -> void override {}
            auto write_sampler_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                          [[maybe_unused]] rhi::sampler_handle sampler) -> void override {}
            auto write_sampled_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                                [[maybe_unused]] rhi::texture_view_handle view,
                                                [[maybe_unused]] rhi::image_layout layout) -> void override {}
            auto write_storage_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                                [[maybe_unused]] rhi::texture_view_handle view,
                                                [[maybe_unused]] rhi::image_layout layout) -> void override {}

            flat_unordered_map<uint64_t, string> object_debug_names;

            auto set_debug_name(rhi::buffer_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::texture_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::texture_view_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::sampler_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::graphics_pipeline_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::compute_pipeline_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::event_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
            auto set_debug_name(rhi::semaphore_handle handle, cstring_view name) -> void override
            {
                object_debug_names[handle.handle] = string{name.data(), name.size()};
            }
        };
    } // namespace

    TEST(executor_test, execute_raster_and_compute_pipeline)
    {
        auto dev = mock_device_with_ports{};
        auto rg = render_graph{1920, 1080};

        struct gbuffer_data
        {
            rg_texture_id albedo;
            rg_texture_id depth;
        };

        bool gbuffer_executed = false;
        rg.add_graphics_pass<gbuffer_data>(
            "GBufferPass",
            [](pass_builder& builder, gbuffer_data& data) {
                const auto alb = builder.create_texture(rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "Albedo",
                });
                const auto dep = builder.create_texture(rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::depth32_float,
                    .name = "Depth",
                });

                data.albedo = builder.set_color_attachment(0, rg_color_attachment{.texture = alb});
                data.depth = builder.set_depth_stencil_attachment(rg_depth_stencil_attachment{.texture = dep});
            },
            [&gbuffer_executed](const gbuffer_data& data, pass_execution_context& ctx,
                               [[maybe_unused]] rhi::command_list& cmd) {
                gbuffer_executed = true;
                const auto alb_h = ctx.get_texture(data.albedo);
                const auto dep_h = ctx.get_texture(data.depth);
                EXPECT_NE(alb_h.handle, 0ULL);
                EXPECT_NE(dep_h.handle, 0ULL);

                const auto alb_sz = ctx.get_resolved_size(data.albedo);
                EXPECT_EQ(alb_sz.width, 1920U);
                EXPECT_EQ(alb_sz.height, 1080U);
            });

        struct lighting_data
        {
            rg_texture_id albedo_in;
            rg_texture_id depth_in;
            rg_texture_id lit_hdr;
        };

        bool lighting_executed = false;
        rg.add_compute_pass<lighting_data>(
            "LightingPass",
            [](pass_builder& builder, lighting_data& data) {
                data.albedo_in = builder.read(rg_texture_id{.id = 0, .version = 1}, rhi::pipeline_stage::compute,
                                              rhi::resource_access::read, rhi::image_layout::general);
                data.depth_in = builder.read(rg_texture_id{.id = 1, .version = 1}, rhi::pipeline_stage::compute,
                                             rhi::resource_access::read, rhi::image_layout::general);

                const auto lit = builder.create_texture(rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .name = "LitHDR",
                });
                data.lit_hdr = builder.write(lit, rhi::pipeline_stage::compute, rhi::resource_access::write,
                                             rhi::image_layout::general);
                builder.mark_sink();
            },
            [&lighting_executed](const lighting_data& data, pass_execution_context& ctx,
                                [[maybe_unused]] rhi::command_list& cmd) {
                lighting_executed = true;
                const auto desc_slot = ctx.get_texture_descriptor(data.albedo_in);
                EXPECT_NE(desc_slot, invalid_descriptor_index);
            });

        const auto exec_res = rg.execute(dev);
        ASSERT_TRUE(exec_res.has_value());

        EXPECT_TRUE(gbuffer_executed);
        EXPECT_TRUE(lighting_executed);

        // Verify command recording: begin/end render pass for GBuffer, pipeline barrier for transitions
        EXPECT_EQ(dev.graphics_port.submit_calls, 1U);
        EXPECT_EQ(dev.graphics_port.cmd.begin_render_pass_calls, 1U);
        EXPECT_EQ(dev.graphics_port.cmd.end_render_pass_calls, 1U);
        EXPECT_GT(dev.graphics_port.cmd.pipeline_barrier_calls, 0U);
    }

    TEST(executor_test, execute_multi_queue_cross_synchronization)
    {
        auto dev = mock_device_with_ports{};
        auto rg = render_graph{1920, 1080};

        struct transfer_data
        {
            rg_buffer_id buf;
        };

        rg.add_transfer_pass<transfer_data>(
            "UploadPass",
            [](pass_builder& builder, transfer_data& data) {
                const auto b = builder.create_buffer(rg_buffer_desc{.size = 1024, .name = "DataBuf"});
                data.buf = builder.write(b, rhi::pipeline_stage::copy, rhi::resource_access::write);
            },
            []([[maybe_unused]] const transfer_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        struct compute_data
        {
            rg_buffer_id buf_in;
        };

        rg.add_compute_pass<compute_data>(
            "ProcessPass",
            [](pass_builder& builder, compute_data& data) {
                data.buf_in = builder.read_write(rg_buffer_id{.id = 0, .version = 1}, rhi::pipeline_stage::compute,
                                                  rhi::resource_access::read_write);
            },
            []([[maybe_unused]] const compute_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        struct graphics_data
        {
            rg_buffer_id buf_read;
        };

        rg.add_graphics_pass<graphics_data>(
            "RenderPass",
            [](pass_builder& builder, graphics_data& data) {
                data.buf_read = builder.read(rg_buffer_id{.id = 0, .version = 2}, rhi::pipeline_stage::vertex,
                                             rhi::resource_access::read);
                builder.mark_sink();
            },
            []([[maybe_unused]] const graphics_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        const auto exec_res = rg.execute(dev);
        ASSERT_TRUE(exec_res.has_value());

        // 3 separate queue submissions: Transfer -> Compute -> Graphics
        EXPECT_EQ(dev.transfer_port.submit_calls, 1U);
        EXPECT_EQ(dev.compute_port.submit_calls, 1U);
        EXPECT_EQ(dev.graphics_port.submit_calls, 1U);

        // Transfer signaled semaphore
        EXPECT_EQ(dev.transfer_port.last_signal_sync.size(), 1U);

        // Compute waited on Transfer semaphore and signaled Graphics semaphore
        EXPECT_EQ(dev.compute_port.last_wait_sync.size(), 1U);
        EXPECT_EQ(dev.compute_port.last_signal_sync.size(), 1U);

        // Graphics waited on Compute semaphore
        EXPECT_EQ(dev.graphics_port.last_wait_sync.size(), 1U);
    }

    TEST(executor_test, execute_conditional_pass_skipped_at_runtime)
    {
        auto dev = mock_device_with_ports{};
        auto rg = render_graph{1920, 1080};

        struct src_data
        {
            rg_texture_id tex;
        };

        rg.add_graphics_pass<src_data>(
            "SourcePass",
            [](pass_builder& builder, src_data& data) {
                const auto t = builder.create_texture(rg_texture_desc{.name = "TexSrc"});
                data.tex = builder.write(t, rhi::pipeline_stage::attachment_output, rhi::resource_access::write,
                                         rhi::image_layout::general);
            },
            [](const src_data&, pass_execution_context&, rhi::command_list&) {});

        struct disabled_data
        {
            rg_texture_id output;
        };

        bool disabled_executed = false;
        rg.add_graphics_pass<disabled_data>(
            "DisabledPass",
            [](pass_builder& builder, disabled_data& data) {
                data.output = builder.passthrough(rg_texture_id{.id = 0, .version = 1});
                builder.set_enable_condition([] { return false; });
            },
            [&disabled_executed](const disabled_data&, pass_execution_context&, rhi::command_list&) {
                disabled_executed = true;
            });

        struct sink_data
        {
            rg_texture_id in_tex;
        };

        bool consumer_executed = false;
        rg.add_graphics_pass<sink_data>(
            "ConsumerPass",
            [](pass_builder& builder, sink_data& data) {
                data.in_tex = builder.read(rg_texture_id{.id = 0, .version = 2}, rhi::pipeline_stage::fragment,
                                           rhi::resource_access::read, rhi::image_layout::general);
                builder.mark_sink();
            },
            [&consumer_executed](const sink_data& data, pass_execution_context& ctx,
                                 [[maybe_unused]] rhi::command_list& cmd) {
                consumer_executed = true;
                const auto handle = ctx.get_texture(data.in_tex);
                EXPECT_NE(handle.handle, 0ULL);
            });

        const auto exec_res = rg.execute(dev);
        ASSERT_TRUE(exec_res.has_value());

        EXPECT_FALSE(disabled_executed);
        EXPECT_TRUE(consumer_executed);
    }

    TEST(executor_test, automatic_pass_debug_regions_and_markers)
    {
        auto dev = mock_device_with_ports{};
        auto rg = render_graph{1920, 1080};

        struct pass_a_data
        {
            rg_texture_id tex;
        };

        rg.add_graphics_pass<pass_a_data>(
            "GeometryPass",
            [](pass_builder& builder, pass_a_data& data) {
                const auto t = builder.create_texture(rg_texture_desc{.name = "GBufferAlbedo"});
                data.tex = builder.write(t, rhi::pipeline_stage::attachment_output, rhi::resource_access::write,
                                         rhi::image_layout::general);
            },
            [](const pass_a_data&, pass_execution_context&, rhi::command_list& cmd) {
                cmd.insert_debug_marker(rhi::debug_label{.name = "DrawMeshes"});
            });

        struct pass_b_data
        {
            rg_texture_id in_tex;
            rg_texture_id out_tex;
        };

        rg.add_compute_pass<pass_b_data>(
            "LightingPass",
            [](pass_builder& builder, pass_b_data& data) {
                data.in_tex = builder.read(rg_texture_id{.id = 0, .version = 1}, rhi::pipeline_stage::compute,
                                           rhi::resource_access::read, rhi::image_layout::general);
                const auto out = builder.create_texture(rg_texture_desc{.name = "HDRColor"});
                data.out_tex = builder.write(out, rhi::pipeline_stage::compute, rhi::resource_access::write,
                                             rhi::image_layout::general);
                builder.mark_sink();
            },
            [](const pass_b_data&, pass_execution_context&, rhi::command_list& cmd) {
                cmd.insert_debug_marker(rhi::debug_label{.name = "DispatchCompute"});
            });

        const auto exec_res = rg.execute(dev);
        ASSERT_TRUE(exec_res.has_value());

#if defined(TEMPEST_ENABLE_DEBUG_MARKERS)
        // Check graphics pass regions and markers
        EXPECT_EQ(dev.graphics_port.cmd.begin_debug_region_calls, 1U);
        EXPECT_EQ(dev.graphics_port.cmd.end_debug_region_calls, 1U);
        ASSERT_GE(dev.graphics_port.cmd.debug_regions.size(), 1U);
        EXPECT_EQ(dev.graphics_port.cmd.debug_regions[0], "GeometryPass");
        ASSERT_GE(dev.graphics_port.cmd.debug_markers.size(), 1U);
        EXPECT_EQ(dev.graphics_port.cmd.debug_markers[0], "DrawMeshes");

        // Check compute pass regions and markers
        EXPECT_EQ(dev.compute_port.cmd.begin_debug_region_calls, 1U);
        EXPECT_EQ(dev.compute_port.cmd.end_debug_region_calls, 1U);
        ASSERT_GE(dev.compute_port.cmd.debug_regions.size(), 1U);
        EXPECT_EQ(dev.compute_port.cmd.debug_regions[0], "LightingPass");
        ASSERT_GE(dev.compute_port.cmd.debug_markers.size(), 1U);
        EXPECT_EQ(dev.compute_port.cmd.debug_markers[0], "DispatchCompute");

        // Check queue batch regions
        EXPECT_GE(dev.graphics_port.begin_debug_region_calls, 1U);
        EXPECT_GE(dev.graphics_port.end_debug_region_calls, 1U);
        EXPECT_GE(dev.compute_port.begin_debug_region_calls, 1U);
        EXPECT_GE(dev.compute_port.end_debug_region_calls, 1U);

        // Check dynamic resource naming
        EXPECT_FALSE(dev.object_debug_names.empty());
#endif
    }
} // namespace tempest::render_graph
