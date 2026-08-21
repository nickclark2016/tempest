#include <gtest/gtest.h>

#include <tempest/render_graph/barrier_solver.hpp>
#include <tempest/render_graph/render_graph.hpp>
#include <tempest/render_graph/temporal_texture.hpp>

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
        };

        class mock_execution_port final : public rhi::execution_port
        {
          public:
            mock_cmd_list cmd;
            uint32_t submit_calls = 0;
            vector<rhi::device_sync_point> last_wait_sync;
            vector<rhi::device_sync_point> last_signal_sync;

            auto wait_idle() -> void override {}

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

        class mock_temporal_device final : public rhi::device
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
        };
    } // namespace

    TEST(temporal_texture_test, initialization_and_indexing)
    {
        auto dev = mock_temporal_device{};
        auto tex = temporal_texture{};

        const auto desc = temporal_texture_desc{
            .desc =
                rg_texture_desc{
                    .size = rg_texture_size::absolute(1920, 1080),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "TestTemporal",
                },
            .history_count = 2, // 3 physical slots
        };

        EXPECT_TRUE(tex.init(dev, desc, 1920, 1080));
        EXPECT_EQ(tex.get_all_textures().size(), 3U);
        EXPECT_EQ(tex.get_all_views().size(), 3U);
        EXPECT_EQ(tex.get_valid_history_count(), 0U);
        EXPECT_FALSE(tex.is_history_valid(1));
        EXPECT_FALSE(tex.is_history_valid(2));

        // Frame 0 write target
        const auto w0 = tex.get_write_texture();
        EXPECT_NE(w0.handle, 0ULL);

        // Advance to Frame 1
        tex.swap();
        EXPECT_EQ(tex.get_valid_history_count(), 1U);
        EXPECT_TRUE(tex.is_history_valid(1));
        EXPECT_FALSE(tex.is_history_valid(2));
        EXPECT_EQ(tex.get_history_texture(1).handle, w0.handle);

        const auto w1 = tex.get_write_texture();
        EXPECT_NE(w1.handle, w0.handle);

        // Advance to Frame 2
        tex.swap();
        EXPECT_EQ(tex.get_valid_history_count(), 2U);
        EXPECT_TRUE(tex.is_history_valid(1));
        EXPECT_TRUE(tex.is_history_valid(2));
        EXPECT_EQ(tex.get_history_texture(1).handle, w1.handle);
        EXPECT_EQ(tex.get_history_texture(2).handle, w0.handle);

        // Advance to Frame 3: valid count clamps to history_count (2)
        tex.swap();
        EXPECT_EQ(tex.get_valid_history_count(), 2U);

        // Invalidate history
        tex.invalidate();
        EXPECT_EQ(tex.get_valid_history_count(), 0U);
        EXPECT_FALSE(tex.is_history_valid(1));

        tex.release(dev);
        EXPECT_FALSE(tex.is_allocated());
    }

    TEST(temporal_texture_test, pass_builder_temporal_binding)
    {
        auto dev = mock_temporal_device{};
        auto tex = temporal_texture{};

        const auto desc = temporal_texture_desc{
            .desc =
                rg_texture_desc{
                    .size = rg_texture_size::absolute(1920, 1080),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "TestTemporal",
                },
            .history_count = 2,
        };
        tex.init(dev, desc, 1920, 1080);

        auto rg = render_graph{1920, 1080};

        struct test_pass_data
        {
            temporal_binding binding;
        };

        // Frame 0: no history
        const auto& p0 = rg.add_compute_pass<test_pass_data>(
            "Pass0",
            [&tex](pass_builder& builder, test_pass_data& data) {
                data.binding = builder.use_temporal_texture(tex, 2);
                builder.mark_sink();
            },
            []([[maybe_unused]] const test_pass_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        EXPECT_FALSE(p0.binding.has_history(1));
        EXPECT_FALSE(p0.binding.has_history(2));
        EXPECT_TRUE(p0.binding.target_write.is_valid());

        // Execute Frame 0 -> triggers automatic tex.swap()
        auto exec_res = rg.execute(dev);
        EXPECT_TRUE(exec_res.has_value());
        EXPECT_EQ(tex.get_valid_history_count(), 1U);

        // Frame 1: 1 frame of history (N-1)
        rg.reset();
        const auto& p1 = rg.add_compute_pass<test_pass_data>(
            "Pass1",
            [&tex](pass_builder& builder, test_pass_data& data) {
                data.binding = builder.use_temporal_texture(tex, 2);
                builder.mark_sink();
            },
            []([[maybe_unused]] const test_pass_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        EXPECT_TRUE(p1.binding.has_history(1));
        EXPECT_FALSE(p1.binding.has_history(2));
        EXPECT_TRUE(p1.binding.get_history(1).is_valid());
        EXPECT_TRUE(p1.binding.target_write.is_valid());

        exec_res = rg.execute(dev);
        EXPECT_TRUE(exec_res.has_value());
        EXPECT_EQ(tex.get_valid_history_count(), 2U);

        // Frame 2: 2 frames of history (N-1 and N-2)
        rg.reset();
        const auto& p2 = rg.add_compute_pass<test_pass_data>(
            "Pass2",
            [&tex](pass_builder& builder, test_pass_data& data) {
                data.binding = builder.use_temporal_texture(tex, 2);
                builder.mark_sink();
            },
            []([[maybe_unused]] const test_pass_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        EXPECT_TRUE(p2.binding.has_history(1));
        EXPECT_TRUE(p2.binding.has_history(2));
        EXPECT_TRUE(p2.binding.get_history(1).is_valid());
        EXPECT_TRUE(p2.binding.get_history(2).is_valid());

        tex.release(dev);
    }

    TEST(temporal_texture_test, barrier_solver_cross_frame_persistence)
    {
        auto dev = mock_temporal_device{};
        auto rg = render_graph{1920, 1080};
        const auto mock_tex_handle = rhi::texture_handle{.handle = 100};
        const auto mock_view_handle = rhi::texture_view_handle{.handle = 200};

        struct frame1_data
        {
            rg_texture_id out;
        };

        // Frame 1: Compute writes to imported texture
        rg.add_compute_pass<frame1_data>(
            "ComputeWritePass",
            [mock_tex_handle, mock_view_handle](pass_builder& builder, frame1_data& data) {
                const auto imported = builder.import_texture(mock_tex_handle, mock_view_handle, rhi::image_layout::undefined);
                data.out = builder.write(imported, rhi::pipeline_stage::compute, rhi::resource_access::write,
                                         rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const frame1_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        auto solver = barrier_solver{};
        const auto compile_res1 = rg.compile();
        EXPECT_TRUE(compile_res1.has_value());
        rg.get_allocator().allocate(dev, compile_res1.value(), rg.get_compiler().get_registered_textures(),
                                    rg.get_compiler().get_registered_buffers(), 1920, 1080);
        [[maybe_unused]] const auto sync_plan1 =
            solver.solve(compile_res1.value(), rg.get_compiler().get_passes(), rg.get_allocator(),
                         rg.get_compiler().get_registered_textures());

        // Frame 2: Graphics vertex shader reads from same imported texture
        rg.reset();
        struct frame2_data
        {
            rg_texture_id in;
        };

        rg.add_graphics_pass<frame2_data>(
            "GraphicsReadPass",
            [mock_tex_handle, mock_view_handle](pass_builder& builder, frame2_data& data) {
                const auto imported = builder.import_texture(mock_tex_handle, mock_view_handle, rhi::image_layout::general);
                data.in = builder.read(imported, rhi::pipeline_stage::vertex, rhi::resource_access::read,
                                       rhi::image_layout::general);
                builder.mark_sink();
            },
            []([[maybe_unused]] const frame2_data& data, [[maybe_unused]] pass_execution_context& ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        const auto compile_res2 = rg.compile();
        EXPECT_TRUE(compile_res2.has_value());
        rg.get_allocator().allocate(dev, compile_res2.value(), rg.get_compiler().get_registered_textures(),
                                    rg.get_compiler().get_registered_buffers(), 1920, 1080);

        // Solve frame 2
        const auto sync_plan = solver.solve(compile_res2.value(), rg.get_compiler().get_passes(), rg.get_allocator(),
                                            rg.get_compiler().get_registered_textures());

        EXPECT_FALSE(sync_plan.pass_plans.empty());
        ASSERT_FALSE(sync_plan.pass_plans[0].texture_barriers.empty());
        EXPECT_TRUE(static_cast<bool>(sync_plan.pass_plans[0].texture_barriers[0].src.stages & rhi::pipeline_stage::compute));
    }

    TEST(temporal_texture_test, clear_temporal_pass)
    {
        auto dev = mock_temporal_device{};
        auto tex = temporal_texture{};

        const auto desc = temporal_texture_desc{
            .desc =
                rg_texture_desc{
                    .size = rg_texture_size::absolute(1920, 1080),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "ClearTest",
                },
            .history_count = 2,
        };
        tex.init(dev, desc, 1920, 1080);
        tex.swap();
        tex.swap();
        EXPECT_EQ(tex.get_valid_history_count(), 2U);

        auto rg = render_graph{1920, 1080};
        rg.add_clear_temporal_pass(tex, rhi::clear_color_value{0.0F, 0.0F, 0.0F, 0.0F});

        EXPECT_EQ(tex.get_valid_history_count(), 0U);
        EXPECT_FALSE(tex.is_history_valid(1));

        const auto compile_res = rg.compile();
        EXPECT_TRUE(compile_res.has_value());
        EXPECT_FALSE(compile_res.value().sorted_pass_indices.empty());

        tex.release(dev);
    }
} // namespace tempest::render_graph
