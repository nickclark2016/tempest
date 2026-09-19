#include <gtest/gtest.h>

#include <tempest/job/job_system.hpp>
#include <tempest/logger.hpp>
#include <tempest/profiler/session.hpp>
#include <tempest/render_graph/flight_texture.hpp>
#include <tempest/render_graph/render_graph.hpp>

namespace tempest::render_graph
{
    namespace
    {
        struct test_context
        {
            logger log{};
            profiler::profiler_session prof{false};
            job::job_system jobs{log, prof,
                                 job::job_system_config{
                                     .performance_worker_count = 0,
                                     .efficiency_worker_count = 0,
                                 }};
        };

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
                                  [[maybe_unused]] span<const rhi::buffer_barrier> buffer_barriers) const
                -> void override
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

            auto push_constants([[maybe_unused]] enum_mask<rhi::shader_stage> stages, [[maybe_unused]] uint32_t offset,
                                [[maybe_unused]] span<const byte> data) -> void override
            {
            }

            auto begin_render_pass([[maybe_unused]] span<const rhi::color_attachment> color_attachments,
                                   [[maybe_unused]] optional<rhi::depth_stencil_attachment> depth_stencil_attachment,
                                   [[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) -> void override
            {
                begin_render_pass_calls++;
            }

            auto end_render_pass() -> void override
            {
                end_render_pass_calls++;
            }

            auto bind_pipeline([[maybe_unused]] rhi::graphics_pipeline_handle pipeline) -> void override
            {
            }
            auto set_viewport([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float width,
                              [[maybe_unused]] float height, [[maybe_unused]] float min_depth,
                              [[maybe_unused]] float max_depth) -> void override
            {
            }
            auto set_scissor([[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y, [[maybe_unused]] uint32_t width,
                             [[maybe_unused]] uint32_t height) -> void override
            {
            }
            auto clear_depth_attachment([[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                                        [[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height,
                                        [[maybe_unused]] float depth) -> void override
            {
            }
            auto clear_stencil_attachment([[maybe_unused]] int32_t x, [[maybe_unused]] int32_t y,
                                          [[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height,
                                          [[maybe_unused]] uint32_t stencil) -> void override
            {
            }
            auto set_depth_bias([[maybe_unused]] float constant_factor, [[maybe_unused]] float clamp,
                                [[maybe_unused]] float slope_factor) -> void override
            {
            }
            auto set_stencil_reference([[maybe_unused]] uint32_t reference) -> void override
            {
            }
            auto set_stencil_compare_mask([[maybe_unused]] uint32_t compare_mask) -> void override
            {
            }
            auto set_stencil_write_mask([[maybe_unused]] uint32_t write_mask) -> void override
            {
            }
            auto bind_index_buffer([[maybe_unused]] rhi::buffer_handle buffer, [[maybe_unused]] rhi::index_type type,
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
                                       [[maybe_unused]] uint32_t draw_count, [[maybe_unused]] uint32_t stride)
                -> void override
            {
            }
            auto draw_indirect_count([[maybe_unused]] rhi::buffer_handle buffer, [[maybe_unused]] uint64_t offset,
                                     [[maybe_unused]] rhi::buffer_handle count_buffer,
                                     [[maybe_unused]] uint64_t count_buffer_offset,
                                     [[maybe_unused]] uint32_t max_draw_count, [[maybe_unused]] uint32_t stride)
                -> void override
            {
            }
            auto draw_indexed_indirect_count([[maybe_unused]] rhi::buffer_handle buffer,
                                             [[maybe_unused]] uint64_t offset,
                                             [[maybe_unused]] rhi::buffer_handle count_buffer,
                                             [[maybe_unused]] uint64_t count_buffer_offset,
                                             [[maybe_unused]] uint32_t max_draw_count, [[maybe_unused]] uint32_t stride)
                -> void override
            {
            }

            auto bind_pipeline([[maybe_unused]] rhi::compute_pipeline_handle pipeline) -> void override
            {
            }
            auto dispatch([[maybe_unused]] uint32_t group_count_x, [[maybe_unused]] uint32_t group_count_y,
                          [[maybe_unused]] uint32_t group_count_z) -> void override
            {
            }
            auto dispatch_indirect([[maybe_unused]] rhi::buffer_handle buffer, [[maybe_unused]] uint64_t offset)
                -> void override
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
            auto blit_texture([[maybe_unused]] rhi::texture_handle src, [[maybe_unused]] rhi::texture_handle dst,
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

            auto wait_idle() -> void override
            {
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

            [[nodiscard]] auto get_timeline_sync_point() const noexcept -> rhi::host_sync_point override
            {
                return {};
            }
        };

        class mock_flight_device final : public rhi::device
        {
          public:
            mock_execution_port graphics_port;
            mock_execution_port compute_port;
            mock_execution_port transfer_port;

            uint64_t next_h = 1;
            uint32_t next_desc = 1;
            rhi::device_desc desc{
                .limits =
                    {
                        .max_image_dimension_2d = 16384,
                    },
            };

            auto wait_idle() -> void override
            {
            }
            auto wait_for_sync([[maybe_unused]] rhi::host_sync_point sync_point) -> void override
            {
            }

            [[nodiscard]] auto get_device_desc() const noexcept -> const rhi::device_desc& override
            {
                return desc;
            }

            [[nodiscard]] auto is_ray_tracing_supported() const -> bool override
            {
                return false;
            }
            [[nodiscard]] auto is_mesh_shading_supported() const -> bool override
            {
                return false;
            }
            [[nodiscard]] auto is_ray_query_supported() const -> bool override
            {
                return false;
            }

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

            auto destroy_render_surface([[maybe_unused]] unique_ptr<rhi::render_surface> surface) -> void override
            {
            }
            auto destroy_raw_surface([[maybe_unused]] rhi::raw_surface_handle surface) -> void override
            {
            }

            [[nodiscard]] auto get_semaphore_value([[maybe_unused]] rhi::semaphore_handle semaphore) const
                -> uint64_t override
            {
                return 0;
            }

            auto signal_semaphore([[maybe_unused]] rhi::semaphore_handle semaphore, [[maybe_unused]] uint64_t value) -> void override
            {
            }

            auto wait_semaphores([[maybe_unused]] span<const rhi::host_sync_point> sync_points,
                                 [[maybe_unused]] uint64_t timeout_ns = ~uint64_t{0},
                                 [[maybe_unused]] bool wait_any = false) -> rhi::wait_status override
            {
                return rhi::wait_status::success;
            }

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

            [[nodiscard]] auto create_buffer([[maybe_unused]] const rhi::buffer_desc& desc)
                -> rhi::buffer_handle override
            {
                return rhi::buffer_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_texture([[maybe_unused]] const rhi::texture_desc& desc)
                -> rhi::texture_handle override
            {
                return rhi::texture_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_texture_view([[maybe_unused]] rhi::texture_handle texture,
                                                   [[maybe_unused]] const rhi::texture_view_desc& desc)
                -> rhi::texture_view_handle override
            {
                return rhi::texture_view_handle{.handle = next_h++};
            }

            [[nodiscard]] auto create_sampler([[maybe_unused]] const rhi::sampler_desc& desc)
                -> rhi::sampler_handle override
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

            uint32_t destroyed_textures{0};
            uint32_t destroyed_views{0};

            auto destroy_buffer([[maybe_unused]] rhi::buffer_handle buffer) -> void override
            {
            }
            auto destroy_texture([[maybe_unused]] rhi::texture_handle texture) -> void override
            {
                ++destroyed_textures;
            }
            auto destroy_texture_view([[maybe_unused]] rhi::texture_view_handle view) -> void override
            {
                ++destroyed_views;
            }
            auto destroy_sampler([[maybe_unused]] rhi::sampler_handle sampler) -> void override
            {
            }
            auto destroy_graphics_pipeline([[maybe_unused]] rhi::graphics_pipeline_handle pipeline) -> void override
            {
            }
            auto destroy_compute_pipeline([[maybe_unused]] rhi::compute_pipeline_handle pipeline) -> void override
            {
            }
            auto destroy_event([[maybe_unused]] rhi::event_handle event) -> void override
            {
            }
            auto destroy_semaphore([[maybe_unused]] rhi::semaphore_handle semaphore) -> void override
            {
            }

            [[nodiscard]] auto allocate_descriptor([[maybe_unused]] rhi::descriptor_type type)
                -> rhi::descriptor_handle override
            {
                return rhi::descriptor_handle{.index = next_desc++, .generation = 1};
            }

            auto free_descriptor([[maybe_unused]] rhi::descriptor_type type,
                                 [[maybe_unused]] rhi::descriptor_handle descriptor) -> void override
            {
            }
            auto write_sampler_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                          [[maybe_unused]] rhi::sampler_handle sampler) -> void override
            {
            }
            auto write_sampled_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                                [[maybe_unused]] rhi::texture_view_handle view,
                                                [[maybe_unused]] rhi::image_layout layout) -> void override
            {
            }
            auto write_storage_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                                [[maybe_unused]] rhi::texture_view_handle view,
                                                [[maybe_unused]] rhi::image_layout layout) -> void override
            {
            }
        };
    } // namespace

    // =========================================================================
    // SECTION: Flight Texture Lifecycle & Indexing Tests
    // =========================================================================

    /// @brief Verifies that flight_texture allocates the requested number of slots,
    ///        correctly maps slot indices with modulo wrapping, and allocates valid sampled descriptors.
    TEST(flight_texture_test, initialization_and_indexing)
    {
        // 1. Setup: Create mock device and flight_texture
        auto dev = mock_flight_device{};
        auto tex = flight_texture{};

        EXPECT_FALSE(tex.is_allocated());
        EXPECT_EQ(tex.get_slot_count(), 0U);

        // 2. Act: Initialize flight_texture with 3 flight slots
        const auto ok = tex.init(dev,
                                 flight_texture_desc{
                                     .desc =
                                         rg_texture_desc{
                                             .size = rg_texture_size::absolute(2048, 2048),
                                             .format = rhi::data_format::depth32_float,
                                             .usage = rhi::texture_usage::depth_stencil_attachment |
                                                      rhi::texture_usage::sampled,
                                             .name = "ShadowMapFlight",
                                         },
                                     .flight_slots = 3,
                                 },
                                 2048, 2048);

        // 3. Assert: Verify allocations and slot mapping
        EXPECT_TRUE(ok);
        EXPECT_TRUE(tex.is_allocated());
        EXPECT_EQ(tex.get_slot_count(), 3U);

        const auto tex0 = tex.get_texture(0);
        const auto tex1 = tex.get_texture(1);
        const auto tex2 = tex.get_texture(2);

        EXPECT_NE(tex0.handle, 0ULL);
        EXPECT_NE(tex1.handle, 0ULL);
        EXPECT_NE(tex2.handle, 0ULL);

        EXPECT_NE(tex0.handle, tex1.handle);
        EXPECT_NE(tex1.handle, tex2.handle);
        EXPECT_NE(tex0.handle, tex2.handle);

        // Modulo wrapping: slot 3 wraps to slot 0, slot 4 wraps to slot 1
        EXPECT_EQ(tex.get_texture(3).handle, tex0.handle);
        EXPECT_EQ(tex.get_texture(4).handle, tex1.handle);

        const auto desc0 = tex.get_sampled_descriptor(0);
        const auto desc1 = tex.get_sampled_descriptor(1);
        const auto desc2 = tex.get_sampled_descriptor(2);

        EXPECT_NE(desc0.index, ~0U);
        EXPECT_NE(desc1.index, ~0U);
        EXPECT_NE(desc2.index, ~0U);
        EXPECT_NE(desc0.index, desc1.index);
    }

    /// @brief Verifies that extracting resources leaves the container unallocated and transfers
    ///        all textures, views, and descriptors into flight_resources for deferred deletion.
    TEST(flight_texture_test, resource_extraction_handover)
    {
        // 1. Setup: Initialize flight_texture with 2 slots
        auto dev = mock_flight_device{};
        auto tex = flight_texture{};

        tex.init(dev,
                 flight_texture_desc{
                     .desc =
                         rg_texture_desc{
                             .size = rg_texture_size::absolute(1024, 1024),
                             .format = rhi::data_format::rgba8_unorm,
                             .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                         },
                     .flight_slots = 2,
                 },
                 1024, 1024);

        ASSERT_TRUE(tex.is_allocated());
        const auto saved_t0 = tex.get_texture(0);
        const auto saved_t1 = tex.get_texture(1);

        // 2. Act: Extract resources
        auto extracted = tex.extract_resources();

        // 3. Assert: Container is empty and resources are preserved in extracted container
        EXPECT_FALSE(tex.is_allocated());
        EXPECT_EQ(tex.get_slot_count(), 0U);
        EXPECT_EQ(extracted.textures.size(), 2U);
        EXPECT_EQ(extracted.views.size(), 2U);
        EXPECT_EQ(extracted.sampled_descriptors.size(), 2U);

        EXPECT_EQ(extracted.textures[0].handle, saved_t0.handle);
        EXPECT_EQ(extracted.textures[1].handle, saved_t1.handle);
    }

    // =========================================================================
    // SECTION: Render Graph Integration Tests
    // =========================================================================

    /// @brief Verifies that pass_builder::set_flight_depth_stencil_attachment imports the specified
    ///        flight slot's texture, binds it as a clear attachment, and allows downstream passes
    ///        in the same frame to read from it without cross-frame hazards.
    TEST(flight_texture_test, pass_builder_flight_depth_stencil_attachment)
    {
        // 1. Setup: Create device, context, flight texture with 2 slots, and render graph
        auto dev = mock_flight_device{};
        auto ctx = test_context{};
        auto tex = flight_texture{};

        tex.init(dev,
                 flight_texture_desc{
                     .desc =
                         rg_texture_desc{
                             .size = rg_texture_size::absolute(2048, 2048),
                             .format = rhi::data_format::depth32_float,
                             .usage = rhi::texture_usage::depth_stencil_attachment | rhi::texture_usage::sampled,
                             .name = "DirectionalShadowMap",
                         },
                     .flight_slots = 2,
                 },
                 2048, 2048);

        auto graph = render_graph{ctx.jobs, 2048, 2048};

        // 2. Act: Record Frame 0 accessing flight slot 0
        struct shadow_data
        {
            rg_texture_id shadow_atlas;
        };

        const auto& shadow_pass_node = graph.add_graphics_pass<shadow_data>(
            "ShadowPass",
            [&tex](pass_builder& builder, shadow_data& data) {
                data.shadow_atlas =
                    builder.set_flight_depth_stencil_attachment(rg_flight_depth_stencil_attachment{
                        .texture = tex,
                        .flight_slot = 0,
                        .depth_load_op = rhi::load_op::clear,
                        .depth_store_op = rhi::store_op::store,
                        .clear_value = {.depth = 0.0F, .stencil = 0},
                    });
            },
            []([[maybe_unused]] const shadow_data& data, [[maybe_unused]] pass_execution_context& exec_ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        const auto shadow_tex = shadow_pass_node.shadow_atlas;

        struct pbr_data
        {
            rg_texture_id sampled_shadow;
        };

        const auto& pbr_pass = graph.add_graphics_pass<pbr_data>(
            "PBROpaquePass",
            [shadow_tex](pass_builder& builder, pbr_data& data) {
                builder.mark_sink();
                data.sampled_shadow = builder.read(shadow_tex, rhi::pipeline_stage::fragment,
                                                   rhi::resource_access::read, rhi::image_layout::general);
            },
            []([[maybe_unused]] const pbr_data& data, [[maybe_unused]] pass_execution_context& exec_ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        // 3. Assert: Compile DAG and verify pass ordering and texture versioning
        const auto compile_res = graph.compile();
        ASSERT_TRUE(compile_res.has_value());

        const auto& dag = compile_res.value();
        EXPECT_EQ(dag.sorted_pass_indices.size(), 2U);
        EXPECT_EQ(dag.sorted_pass_indices[0], 0U); // ShadowPass executes first
        EXPECT_EQ(dag.sorted_pass_indices[1], 1U); // PBROpaquePass executes second

        EXPECT_TRUE(pbr_pass.sampled_shadow.is_valid());
        EXPECT_EQ(pbr_pass.sampled_shadow.id, shadow_tex.id);
    }

    /// @brief Verifies that pass_builder::set_flight_color_attachment imports the specified flight slot
    ///        and marks the attachment output appropriately.
    TEST(flight_texture_test, pass_builder_flight_color_attachment)
    {
        // 1. Setup: Create device, context, flight texture, and render graph
        auto dev = mock_flight_device{};
        auto ctx = test_context{};
        auto tex = flight_texture{};

        tex.init(dev,
                 flight_texture_desc{
                     .desc =
                         rg_texture_desc{
                             .size = rg_texture_size::absolute(1920, 1080),
                             .format = rhi::data_format::rgba16_float,
                             .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                             .name = "ColorTargetFlight",
                         },
                     .flight_slots = 2,
                 },
                 1920, 1080);

        auto graph = render_graph{ctx.jobs, 1920, 1080};

        // 2. Act: Record a color write pass using flight slot 1
        struct draw_data
        {
            rg_texture_id out_color;
        };

        const auto& draw_pass = graph.add_graphics_pass<draw_data>(
            "ColorPass",
            [&tex](pass_builder& builder, draw_data& data) {
                data.out_color = builder.set_flight_color_attachment(0, rg_flight_color_attachment{
                                                                            .texture = tex,
                                                                            .flight_slot = 1,
                                                                            .load_op = rhi::load_op::clear,
                                                                            .store_op = rhi::store_op::store,
                                                                        });
            },
            []([[maybe_unused]] const draw_data& data, [[maybe_unused]] pass_execution_context& exec_ctx,
               [[maybe_unused]] rhi::command_list& cmd) {});

        // 3. Assert: Compile DAG and verify pass is preserved as sink
        const auto compile_res = graph.compile();
        ASSERT_TRUE(compile_res.has_value());

        const auto& dag = compile_res.value();
        EXPECT_EQ(dag.sorted_pass_indices.size(), 1U);
        EXPECT_TRUE(draw_pass.out_color.is_valid());
    }
} // namespace tempest::render_graph
