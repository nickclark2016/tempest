#include <gtest/gtest.h>

#include <tempest/render_graph/barrier_solver.hpp>

namespace tempest::render_graph
{
    namespace
    {
        class mock_test_device final : public rhi::device
        {
          public:
            uint64_t next_h = 1;

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

            [[nodiscard]] auto get_graphics_execution_port() -> rhi::execution_port& override
            {
                return *reinterpret_cast<rhi::execution_port*>(this);
            }

            [[nodiscard]] auto get_async_compute_execution_port() -> rhi::execution_port& override
            {
                return *reinterpret_cast<rhi::execution_port*>(this);
            }

            [[nodiscard]] auto get_async_transfer_execution_port() -> rhi::execution_port& override
            {
                return *reinterpret_cast<rhi::execution_port*>(this);
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
                return rhi::descriptor_handle{.index = 1, .generation = 1};
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

    TEST(barrier_solver_test, texture_layout_transition_and_hazard_barriers)
    {
        auto dev = mock_test_device{};
        auto allocator = transient_allocator{};

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc = rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "ColorHDR",
                },
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 3};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U, 1U, 2U, 3U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes),
            .buffer_lifetimes = {},
        };

        allocator.allocate(dev, dag, textures, {}, 1920, 1080);

        // Define 4 sequential passes touching ColorHDR
        const auto passes = vector<pass_node>{
            init_list,
            // Pass 0: GBuffer write
            pass_node{
                .name = "GBufferPass",
                .pass_index = 0,
                .queue = queue_type::graphics,
                .texture_accesses = vector<texture_access>{
                    init_list,
                    texture_access{
                        .texture = rg_texture_id{.id = 0, .version = 0},
                        .type = access_type::write,
                        .stages = rhi::pipeline_stage::attachment_output,
                        .access = rhi::resource_access::write,
                        .layout = rhi::image_layout::general,
                    },
                },
            },
            // Pass 1: Lighting compute read
            pass_node{
                .name = "LightingPass",
                .pass_index = 1,
                .queue = queue_type::graphics,
                .texture_accesses = vector<texture_access>{
                    init_list,
                    texture_access{
                        .texture = rg_texture_id{.id = 0, .version = 1},
                        .type = access_type::read,
                        .stages = rhi::pipeline_stage::compute,
                        .access = rhi::resource_access::read,
                        .layout = rhi::image_layout::general,
                    },
                },
            },
            // Pass 2: PostProcess fragment read (Read-after-Read on same queue and layout: NO BARRIER)
            pass_node{
                .name = "PostProcessPass",
                .pass_index = 2,
                .queue = queue_type::graphics,
                .texture_accesses = vector<texture_access>{
                    init_list,
                    texture_access{
                        .texture = rg_texture_id{.id = 0, .version = 1},
                        .type = access_type::read,
                        .stages = rhi::pipeline_stage::fragment,
                        .access = rhi::resource_access::read,
                        .layout = rhi::image_layout::general,
                    },
                },
            },
            // Pass 3: Present blit read (Layout transition from general to present)
            pass_node{
                .name = "PresentPass",
                .pass_index = 3,
                .queue = queue_type::graphics,
                .texture_accesses = vector<texture_access>{
                    init_list,
                    texture_access{
                        .texture = rg_texture_id{.id = 0, .version = 1},
                        .type = access_type::read,
                        .stages = rhi::pipeline_stage::blit,
                        .access = rhi::resource_access::read,
                        .layout = rhi::image_layout::present,
                    },
                },
            },
        };

        auto solver = barrier_solver{};
        const auto sync = solver.solve(dag, passes, allocator, textures);

        ASSERT_EQ(sync.pass_plans.size(), 4U);

        // Pass 0: initial undefined -> general transition
        EXPECT_EQ(sync.pass_plans[0].texture_barriers.size(), 1U);
        EXPECT_EQ(sync.pass_plans[0].texture_barriers[0].src.layout, rhi::image_layout::undefined);
        EXPECT_EQ(sync.pass_plans[0].texture_barriers[0].dst.layout, rhi::image_layout::general);
        EXPECT_EQ(sync.pass_plans[0].texture_barriers[0].dst.stages, rhi::pipeline_stage::attachment_output);

        // Pass 1: RaW hazard (attachment_output -> compute)
        EXPECT_EQ(sync.pass_plans[1].texture_barriers.size(), 1U);
        EXPECT_EQ(sync.pass_plans[1].texture_barriers[0].src.stages, rhi::pipeline_stage::attachment_output);
        EXPECT_EQ(sync.pass_plans[1].texture_barriers[0].dst.stages, rhi::pipeline_stage::compute);

        // Pass 2: Read-after-Read (redundant, elided!)
        EXPECT_EQ(sync.pass_plans[2].texture_barriers.size(), 0U);

        // Pass 3: general -> present layout transition
        EXPECT_EQ(sync.pass_plans[3].texture_barriers.size(), 1U);
        EXPECT_EQ(sync.pass_plans[3].texture_barriers[0].src.layout, rhi::image_layout::general);
        EXPECT_EQ(sync.pass_plans[3].texture_barriers[0].dst.layout, rhi::image_layout::present);
    }

    TEST(barrier_solver_test, buffer_waw_and_raw_barriers)
    {
        auto dev = mock_test_device{};
        auto allocator = transient_allocator{};

        const auto buffers = vector<registered_buffer>{
            init_list,
            registered_buffer{
                .id = 0,
                .desc = rg_buffer_desc{.size = 4096, .name = "SSBO"},
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 2};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U, 1U, 2U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = {},
            .buffer_lifetimes = tempest::move(lifetimes),
        };

        allocator.allocate(dev, dag, {}, buffers, 1920, 1080);

        const auto passes = vector<pass_node>{
            init_list,
            // Pass 0: Compute write
            pass_node{
                .name = "ComputePass",
                .pass_index = 0,
                .queue = queue_type::graphics,
                .buffer_accesses = vector<buffer_access>{
                    init_list,
                    buffer_access{
                        .buffer = rg_buffer_id{.id = 0, .version = 0},
                        .type = access_type::write,
                        .stages = rhi::pipeline_stage::compute,
                        .access = rhi::resource_access::write,
                    },
                },
            },
            // Pass 1: Copy read (RaW hazard)
            pass_node{
                .name = "CopyPass",
                .pass_index = 1,
                .queue = queue_type::graphics,
                .buffer_accesses = vector<buffer_access>{
                    init_list,
                    buffer_access{
                        .buffer = rg_buffer_id{.id = 0, .version = 1},
                        .type = access_type::read,
                        .stages = rhi::pipeline_stage::copy,
                        .access = rhi::resource_access::read,
                    },
                },
            },
            // Pass 2: Transfer read again (Read-after-Read: elided)
            pass_node{
                .name = "ReadPass2",
                .pass_index = 2,
                .queue = queue_type::graphics,
                .buffer_accesses = vector<buffer_access>{
                    init_list,
                    buffer_access{
                        .buffer = rg_buffer_id{.id = 0, .version = 1},
                        .type = access_type::read,
                        .stages = rhi::pipeline_stage::copy,
                        .access = rhi::resource_access::read,
                    },
                },
            },
        };

        auto solver = barrier_solver{};
        const auto sync = solver.solve(dag, passes, allocator, {});

        ASSERT_EQ(sync.pass_plans.size(), 3U);
        // Pass 0: Initial write barrier
        EXPECT_EQ(sync.pass_plans[0].buffer_barriers.size(), 1U);
        EXPECT_EQ(sync.pass_plans[0].buffer_barriers[0].dst.stages, rhi::pipeline_stage::compute);

        // Pass 1: RaW hazard compute -> copy
        EXPECT_EQ(sync.pass_plans[1].buffer_barriers.size(), 1U);
        EXPECT_EQ(sync.pass_plans[1].buffer_barriers[0].src.stages, rhi::pipeline_stage::compute);
        EXPECT_EQ(sync.pass_plans[1].buffer_barriers[0].dst.stages, rhi::pipeline_stage::copy);

        // Pass 2: Read-after-Read (elided)
        EXPECT_EQ(sync.pass_plans[2].buffer_barriers.size(), 0U);
    }

    TEST(barrier_solver_test, queue_family_batching_and_transitions)
    {
        const auto passes = vector<pass_node>{
            init_list,
            pass_node{.name = "Pass0", .pass_index = 0, .queue = queue_type::graphics},
            pass_node{.name = "Pass1", .pass_index = 1, .queue = queue_type::graphics},
            pass_node{.name = "Pass2", .pass_index = 2, .queue = queue_type::async_compute},
            pass_node{.name = "Pass3", .pass_index = 3, .queue = queue_type::async_compute},
            pass_node{.name = "Pass4", .pass_index = 4, .queue = queue_type::graphics},
        };

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U, 1U, 2U, 3U, 4U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = {},
            .buffer_lifetimes = {},
        };

        const auto allocator = transient_allocator{};
        auto solver = barrier_solver{};
        const auto sync = solver.solve(dag, passes, allocator, {});

        ASSERT_EQ(sync.queue_batches.size(), 3U);

        // Batch 0: Graphics [0, 1]
        EXPECT_EQ(sync.queue_batches[0].queue, queue_type::graphics);
        ASSERT_EQ(sync.queue_batches[0].pass_indices.size(), 2U);
        EXPECT_EQ(sync.queue_batches[0].pass_indices[0], 0U);
        EXPECT_EQ(sync.queue_batches[0].pass_indices[1], 1U);

        // Batch 1: Async Compute [2, 3]
        EXPECT_EQ(sync.queue_batches[1].queue, queue_type::async_compute);
        ASSERT_EQ(sync.queue_batches[1].pass_indices.size(), 2U);
        EXPECT_EQ(sync.queue_batches[1].pass_indices[0], 2U);
        EXPECT_EQ(sync.queue_batches[1].pass_indices[1], 3U);

        // Batch 2: Graphics [4]
        EXPECT_EQ(sync.queue_batches[2].queue, queue_type::graphics);
        ASSERT_EQ(sync.queue_batches[2].pass_indices.size(), 1U);
        EXPECT_EQ(sync.queue_batches[2].pass_indices[0], 4U);
    }
} // namespace tempest::render_graph
