#include <gtest/gtest.h>

#include <tempest/render_graph/transient_allocator.hpp>

namespace tempest::render_graph
{
    namespace
    {
        class mock_device final : public rhi::device
        {
          public:
            uint32_t created_textures = 0;
            uint32_t destroyed_textures = 0;
            uint32_t created_views = 0;
            uint32_t destroyed_views = 0;
            uint32_t created_buffers = 0;
            uint32_t destroyed_buffers = 0;
            uint32_t allocated_descriptors = 0;
            uint32_t freed_descriptors = 0;

            uint64_t next_handle = 1;
            uint32_t next_desc_index = 1;
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
                return rhi::raw_surface_handle{.handle = next_handle++};
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

            [[nodiscard]] auto create_buffer([[maybe_unused]] const rhi::buffer_desc& desc)
                -> rhi::buffer_handle override
            {
                ++created_buffers;
                const auto h = next_handle++;
                return rhi::buffer_handle{.handle = h, .gpu_address = h * 0x10000ULL};
            }

            [[nodiscard]] auto create_texture([[maybe_unused]] const rhi::texture_desc& desc)
                -> rhi::texture_handle override
            {
                ++created_textures;
                return rhi::texture_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_texture_view([[maybe_unused]] rhi::texture_handle texture,
                                                   [[maybe_unused]] const rhi::texture_view_desc& desc)
                -> rhi::texture_view_handle override
            {
                ++created_views;
                return rhi::texture_view_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_sampler([[maybe_unused]] const rhi::sampler_desc& desc)
                -> rhi::sampler_handle override
            {
                return rhi::sampler_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_graphics_pipeline([[maybe_unused]] const rhi::graphics_pipeline_desc& desc)
                -> rhi::graphics_pipeline_handle override
            {
                return rhi::graphics_pipeline_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_compute_pipeline([[maybe_unused]] const rhi::compute_pipeline_desc& desc)
                -> rhi::compute_pipeline_handle override
            {
                return rhi::compute_pipeline_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_event() -> rhi::event_handle override
            {
                return rhi::event_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_timeline_semaphore() -> rhi::semaphore_handle override
            {
                return rhi::semaphore_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_binary_semaphore() -> rhi::semaphore_handle override
            {
                return rhi::semaphore_handle{.handle = next_handle++};
            }

            auto destroy_buffer([[maybe_unused]] rhi::buffer_handle buffer) -> void override
            {
                ++destroyed_buffers;
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
                ++allocated_descriptors;
                return rhi::descriptor_handle{.index = next_desc_index++, .generation = 1};
            }

            auto free_descriptor([[maybe_unused]] rhi::descriptor_type type,
                                 [[maybe_unused]] rhi::descriptor_handle descriptor) -> void override
            {
                ++freed_descriptors;
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

    TEST(transient_allocator_test, non_overlapping_texture_reuse)
    {
        auto dev = mock_device{};
        auto allocator = transient_allocator{};

        // Registered textures: A and B are identical in format and size (surface relative 1.0x 1.0x)
        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "TexA",
                    },
            },
            registered_texture{
                .id = 1,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "TexB",
                    },
            },
        };

        // Non-overlapping lifetimes: TexA is used in pass 0 [0, 0], TexB in pass 1 [1, 1]
        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes[1] = resource_lifetime{.first_pass = 1, .last_pass = 1};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U, 1U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes),
            .buffer_lifetimes = {},
        };

        allocator.allocate(dev, dag, textures, {}, 1920, 1080);

        // Verify that only 1 physical texture was created and both TexA and TexB share it
        EXPECT_EQ(dev.created_textures, 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);

        const auto* alloc_a = allocator.get_texture(0);
        const auto* alloc_b = allocator.get_texture(1);

        ASSERT_NE(alloc_a, nullptr);
        ASSERT_NE(alloc_b, nullptr);
        EXPECT_EQ(alloc_a->handle.handle, alloc_b->handle.handle);
        EXPECT_EQ(alloc_a->default_view.handle, alloc_b->default_view.handle);
    }

    TEST(transient_allocator_test, overlapping_textures_allocate_disjoint)
    {
        auto dev = mock_device{};
        auto allocator = transient_allocator{};

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "TexA",
                    },
            },
            registered_texture{
                .id = 1,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "TexB",
                    },
            },
        };

        // Overlapping lifetimes: both active during pass 0 to pass 1
        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 1};
        lifetimes[1] = resource_lifetime{.first_pass = 0, .last_pass = 1};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U, 1U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes),
            .buffer_lifetimes = {},
        };

        allocator.allocate(dev, dag, textures, {}, 1920, 1080);

        // Lifetimes overlap, so 2 distinct textures must be allocated
        EXPECT_EQ(dev.created_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        const auto* alloc_a = allocator.get_texture(0);
        const auto* alloc_b = allocator.get_texture(1);

        ASSERT_NE(alloc_a, nullptr);
        ASSERT_NE(alloc_b, nullptr);
        EXPECT_NE(alloc_a->handle.handle, alloc_b->handle.handle);
    }

    TEST(transient_allocator_test, surface_resize_preserves_absolute_textures)
    {
        auto dev = mock_device{};
        auto allocator = transient_allocator{};

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "SurfaceRelativeTex",
                    },
            },
            registered_texture{
                .id = 1,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::absolute(512, 512),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "FixedTex",
                    },
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes[1] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes),
            .buffer_lifetimes = {},
        };

        // Frame 1: 1080p
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        // Surface resize event (1080p -> 1440p)
        allocator.on_surface_resize(dev);
        // Destruction of surface-relative texture is deferred until flight slot arrives
        EXPECT_EQ(dev.destroyed_textures, 0U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U); // FixedTex remains in active pool!

        // Frame 2: 1440p (2560x1440)
        auto lifetimes2 = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes2[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes2[1] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag2 = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes2),
            .buffer_lifetimes = {},
        };

        allocator.allocate(dev, dag2, textures, {}, 2560, 1440);

        // Retired texture was safely reclaimed and destroyed when slot arrived
        EXPECT_EQ(dev.destroyed_textures, 1U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 0U);
        // Fixed texture was reused, only 1 new surface texture was created (total 3 created)
        EXPECT_EQ(dev.created_textures, 3U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        const auto* alloc_fixed = allocator.get_texture(1);
        ASSERT_NE(alloc_fixed, nullptr);
        EXPECT_EQ(alloc_fixed->size.width, 512U);
        EXPECT_EQ(alloc_fixed->size.height, 512U);

        const auto* alloc_surface = allocator.get_texture(0);
        ASSERT_NE(alloc_surface, nullptr);
        EXPECT_EQ(alloc_surface->size.width, 2560U);
        EXPECT_EQ(alloc_surface->size.height, 1440U);

        // Full release cleanup
        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_textures, 3U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 0U);
    }

    TEST(transient_allocator_test, buffer_reuse_and_cleanup)
    {
        auto dev = mock_device{};
        auto allocator = transient_allocator{};

        const auto buffers = vector<registered_buffer>{
            init_list,
            registered_buffer{
                .id = 0,
                .desc = rg_buffer_desc{.size = 2048, .name = "BufA"},
            },
            registered_buffer{
                .id = 1,
                .desc = rg_buffer_desc{.size = 2048, .name = "BufB"},
            },
        };

        // Non-overlapping: BufA [0, 0], BufB [1, 1]
        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes[1] = resource_lifetime{.first_pass = 1, .last_pass = 1};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U, 1U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = {},
            .buffer_lifetimes = tempest::move(lifetimes),
        };

        allocator.allocate(dev, dag, {}, buffers, 1920, 1080);

        EXPECT_EQ(dev.created_buffers, 1U);
        EXPECT_EQ(allocator.get_buffer_pool_count(), 1U);

        const auto* alloc_a = allocator.get_buffer(0);
        const auto* alloc_b = allocator.get_buffer(1);

        ASSERT_NE(alloc_a, nullptr);
        ASSERT_NE(alloc_b, nullptr);
        EXPECT_EQ(alloc_a->handle.handle, alloc_b->handle.handle);

        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_buffers, 1U);
        EXPECT_EQ(allocator.get_buffer_pool_count(), 0U);
    }

    TEST(transient_allocator_test, multi_frame_in_flight_pool_isolation)
    {
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        allocator.set_frames_in_flight(2);

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "ColorTarget",
                    },
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = lifetimes,
            .buffer_lifetimes = {},
        };

        // Frame 0 (slot 0)
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 1U);
        const auto* tex_slot0 = allocator.get_texture(0);
        ASSERT_NE(tex_slot0, nullptr);
        const auto handle_slot0 = tex_slot0->handle.handle;

        // Frame 1 (slot 1) - running concurrently, must not recycle slot 0's active texture
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 2U);
        const auto* tex_slot1 = allocator.get_texture(0);
        ASSERT_NE(tex_slot1, nullptr);
        const auto handle_slot1 = tex_slot1->handle.handle;
        EXPECT_NE(handle_slot0, handle_slot1);

        // Frame 2 (slot 0) - previous frame 0 has retired, so slot 0 can recycle its own texture
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 2U); // No new texture created
        const auto* tex_slot0_reused = allocator.get_texture(0);
        ASSERT_NE(tex_slot0_reused, nullptr);
        EXPECT_EQ(tex_slot0_reused->handle.handle, handle_slot0);

        // Frame 3 (slot 1) - slot 1 recycles its texture
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 2U); // No new texture created
        const auto* tex_slot1_reused = allocator.get_texture(0);
        ASSERT_NE(tex_slot1_reused, nullptr);
        EXPECT_EQ(tex_slot1_reused->handle.handle, handle_slot1);

        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_textures, 2U);
    }

    /// @brief Verifies that allocated transient buffers and imported buffers expose their non-zero GPU device
    /// addresses.
    TEST(transient_allocator_test, buffer_device_address_propagation)
    {
        // Setup: Mock device and transient allocator
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        allocator.set_frames_in_flight(1);
        allocator.set_frame_slot(0);

        const auto imported_gpu_addr = 0xDEADBEEF0000ULL;

        // Act: Register 1 transient buffer and 1 imported buffer
        const auto buffers = vector<registered_buffer>{
            init_list,
            registered_buffer{
                .id = 0,
                .desc =
                    rg_buffer_desc{
                        .size = 1024,
                        .usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address,
                        .name = "TransientStorageBuffer",
                    },
                .is_imported = false,
            },
            registered_buffer{
                .id = 1,
                .desc =
                    rg_buffer_desc{
                        .size = 2048,
                        .name = "ImportedBuffer",
                    },
                .is_imported = true,
                .imported_handle =
                    rhi::buffer_handle{
                        .handle = 999,
                        .gpu_address = imported_gpu_addr,
                    },
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes[1] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = {},
            .buffer_lifetimes = tempest::move(lifetimes),
        };

        allocator.allocate(dev, dag, {}, buffers, 1920, 1080);

        // Assert: Both buffers have valid non-zero device addresses matching their physical GPU handles
        const auto* transient_alloc = allocator.get_buffer(0);
        ASSERT_NE(transient_alloc, nullptr);
        EXPECT_NE(transient_alloc->device_address, 0ULL);
        EXPECT_EQ(transient_alloc->device_address, transient_alloc->handle.gpu_address);

        const auto* imported_alloc = allocator.get_buffer(1);
        ASSERT_NE(imported_alloc, nullptr);
        EXPECT_EQ(imported_alloc->device_address, imported_gpu_addr);

        // Teardown
        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_buffers, 1U);
    }

    /// @brief Tests that surface-relative textures with mismatched dimensions are evicted after their flight cycle on
    /// resize.
    TEST(transient_allocator_test, stale_surface_relative_texture_eviction_on_resize)
    {
        // Setup: Mock device and transient allocator with frames_in_flight = 2
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        allocator.set_frames_in_flight(2);
        allocator.set_pool_config(transient_pool_config{
            .max_unused_cycles = 2,
            .evict_mismatched_dimensions = true,
        });

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                        .name = "ViewportColorTarget",
                    },
                .is_imported = false,
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes),
            .buffer_lifetimes = {},
        };

        // Act: Render at 1280x720 across 2 frames in flight
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1280, 720);
        EXPECT_EQ(dev.created_textures, 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);

        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1280, 720);
        EXPECT_EQ(dev.created_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        // Act: Resize viewport to 1920x1080 on frame slot 0
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        // Slot 0 allocated a new 1920x1080 texture and evicted the old 1280x720 texture from slot 0
        EXPECT_EQ(dev.created_textures, 3U);
        EXPECT_EQ(dev.destroyed_textures, 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U); // 1 stale from slot 1, 1 new from slot 0

        // Act: Next frame on frame slot 1 at 1920x1080
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        // Slot 1 allocated a new 1920x1080 texture and evicted the old 1280x720 texture from slot 1
        EXPECT_EQ(dev.created_textures, 4U);
        EXPECT_EQ(dev.destroyed_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U); // Only the 2 new 1920x1080 textures remain in pool

        // Subsequent frames at 1920x1080 recycle smoothly with zero allocations or destructions
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 4U);
        EXPECT_EQ(dev.destroyed_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 4U);
        EXPECT_EQ(dev.destroyed_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        // Teardown
        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_textures, 4U);
    }

    /// @brief Tests that unused transient buffers are aged out and evicted after max_unused_cycles.
    TEST(transient_allocator_test, unused_buffer_eviction_after_max_cycles)
    {
        // Setup: Mock device and transient allocator with max_unused_cycles = 2
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        allocator.set_frames_in_flight(1);
        allocator.set_frame_slot(0);
        allocator.set_pool_config(transient_pool_config{
            .max_unused_cycles = 2,
            .evict_mismatched_dimensions = true,
        });

        const auto buffers = vector<registered_buffer>{
            init_list,
            registered_buffer{
                .id = 0,
                .desc =
                    rg_buffer_desc{
                        .size = 4096,
                        .usage = rhi::buffer_usage::storage_buffer,
                        .name = "TemporaryBuffer",
                    },
                .is_imported = false,
            },
        };

        auto active_lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        active_lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag_with_buffer = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = {},
            .buffer_lifetimes = tempest::move(active_lifetimes),
        };

        const auto empty_dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = {},
            .buffer_lifetimes = {},
        };

        // Act: Allocate buffer in frame 0
        allocator.allocate(dev, dag_with_buffer, {}, buffers, 1920, 1080);
        EXPECT_EQ(dev.created_buffers, 1U);
        EXPECT_EQ(allocator.get_buffer_pool_count(), 1U);

        // Act: Frame 1 with empty DAG (unused cycle 1, should NOT evict yet since max_unused_cycles = 2)
        allocator.allocate(dev, empty_dag, {}, buffers, 1920, 1080);
        EXPECT_EQ(dev.destroyed_buffers, 0U);
        EXPECT_EQ(allocator.get_buffer_pool_count(), 1U);

        // Act: Frame 2 with empty DAG (unused cycle 2, hits max_unused_cycles, evicts buffer)
        allocator.allocate(dev, empty_dag, {}, buffers, 1920, 1080);
        EXPECT_EQ(dev.destroyed_buffers, 1U);
        EXPECT_EQ(allocator.get_buffer_pool_count(), 0U);

        // Teardown
        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_buffers, 1U);
    }

    /// @brief Tests that surface resize defers eviction until the respective flight slot runs allocate.
    TEST(transient_allocator_test, transient_allocator_resize_deferred_eviction)
    {
        // Setup: Mock device and transient allocator with 2 frames in flight
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        allocator.set_frames_in_flight(2);

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .usage = rhi::texture_usage::color_attachment | rhi::texture_usage::sampled,
                        .name = "SurfaceTex",
                    },
                .is_imported = false,
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = tempest::move(lifetimes),
            .buffer_lifetimes = {},
        };

        // Act: Frame slot 0 allocation at 1080p
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);

        // Act: Frame slot 1 allocation at 1080p
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        // Act: Surface resize occurs (1080p -> 1440p)
        allocator.on_surface_resize(dev);

        // Assert: Both surface textures moved to retired; 0 textures destroyed immediately
        EXPECT_EQ(dev.destroyed_textures, 0U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 0U);

        // Act: Advance to slot 0 and allocate at 1440p
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 2560, 1440);

        // Assert: Slot 0 retired texture is reclaimed and destroyed, slot 1 retired texture remains
        EXPECT_EQ(dev.destroyed_textures, 1U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);

        // Act: Advance to slot 1 and allocate at 1440p
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 2560, 1440);

        // Assert: Slot 1 retired texture is reclaimed and destroyed; retired pool is now empty
        EXPECT_EQ(dev.destroyed_textures, 2U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 0U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 2U);

        // Teardown
        allocator.release_all(dev);
        EXPECT_EQ(dev.destroyed_textures, 4U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 0U);
    }

    /// @brief Stress-tests rapid repeated surface resize across multiple frames in flight,
    /// verifying deferred retirement isolation, fixed-texture preservation, and clean teardown.
    TEST(transient_allocator_test, rapid_repeated_resize_multi_flight_stress)
    {
        // Setup: Initialize mock device and allocator with 4 frames in flight
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        constexpr auto flight_slot_count = 4U;
        allocator.set_frames_in_flight(flight_slot_count);

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "SurfaceTexA",
                    },
            },
            registered_texture{
                .id = 1,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(0.5F, 0.5F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "SurfaceTexB",
                    },
            },
            registered_texture{
                .id = 2,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::absolute(512, 512),
                        .format = rhi::data_format::r8_unorm,
                        .name = "FixedTex",
                    },
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes[1] = resource_lifetime{.first_pass = 0, .last_pass = 0};
        lifetimes[2] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = lifetimes,
            .buffer_lifetimes = {},
        };

        // Act: Run 16 consecutive frames with varying surface dimensions and rapid resize events
        constexpr auto total_iterations = 16U;
        for (auto frame_idx = 0U; frame_idx < total_iterations; ++frame_idx)
        {
            const auto current_slot = frame_idx % flight_slot_count;
            allocator.set_frame_slot(current_slot);

            const auto width = 1280U + (frame_idx * 32U);
            const auto height = 720U + (frame_idx * 16U);

            // Trigger surface resize event
            allocator.on_surface_resize(dev);

            // Multiple rapid resizes in the same frame should not leak or double-retire
            if (frame_idx % 2 == 0)
            {
                allocator.on_surface_resize(dev);
            }

            allocator.allocate(dev, dag, textures, {}, width, height);

            // Assert: active textures are accessible and properly sized
            const auto* alloc_a = allocator.get_texture(0);
            const auto* alloc_b = allocator.get_texture(1);
            const auto* alloc_fixed = allocator.get_texture(2);

            ASSERT_NE(alloc_a, nullptr);
            ASSERT_NE(alloc_b, nullptr);
            ASSERT_NE(alloc_fixed, nullptr);

            EXPECT_EQ(alloc_a->size.width, width);
            EXPECT_EQ(alloc_a->size.height, height);
            EXPECT_EQ(alloc_b->size.width, width / 2U);
            EXPECT_EQ(alloc_b->size.height, height / 2U);
            EXPECT_EQ(alloc_fixed->size.width, 512U);
            EXPECT_EQ(alloc_fixed->size.height, 512U);
        }

        // Teardown: release all resources and verify clean destruction of active and retired
        allocator.release_all(dev);

        EXPECT_EQ(dev.created_textures, dev.destroyed_textures);
        EXPECT_EQ(dev.created_views, dev.destroyed_views);
        EXPECT_EQ(dev.allocated_descriptors, dev.freed_descriptors);
        EXPECT_EQ(allocator.get_texture_pool_count(), 0U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 0U);
    }

    /// @brief Stress-tests delayed flight slot advancement with unaligned resize triggers,
    /// verifying exact flight slot isolation and zero premature destructions.
    TEST(transient_allocator_test, delayed_timeline_flight_slot_reclamation_stress)
    {
        // Setup: 3 frames in flight
        auto dev = mock_device{};
        auto allocator = transient_allocator{};
        constexpr auto flight_slot_count = 3U;
        allocator.set_frames_in_flight(flight_slot_count);

        const auto textures = vector<registered_texture>{
            init_list,
            registered_texture{
                .id = 0,
                .desc =
                    rg_texture_desc{
                        .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                        .format = rhi::data_format::rgba8_unorm,
                        .name = "SurfaceTex",
                    },
            },
        };

        auto lifetimes = flat_unordered_map<uint32_t, resource_lifetime>{};
        lifetimes[0] = resource_lifetime{.first_pass = 0, .last_pass = 0};

        const auto dag = compiled_dag{
            .sorted_pass_indices = vector<uint32_t>{init_list, 0U},
            .resolved_texture_aliases = {},
            .resolved_buffer_aliases = {},
            .texture_lifetimes = lifetimes,
            .buffer_lifetimes = {},
        };

        // Act: Slot 0 allocation at 1080p
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1920, 1080);
        EXPECT_EQ(dev.created_textures, 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);

        // Resize 1 (1080p -> 1440p)
        allocator.on_surface_resize(dev);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 1U);
        EXPECT_EQ(dev.destroyed_textures, 0U);

        // Act: Slot 1 allocation at 1440p
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 2560, 1440);
        EXPECT_EQ(dev.created_textures, 2U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 1U);
        EXPECT_EQ(dev.destroyed_textures, 0U);

        // Resize 2 (1440p -> 4K)
        allocator.on_surface_resize(dev);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 2U);
        EXPECT_EQ(dev.destroyed_textures, 0U);

        // Act: Slot 2 allocation at 4K
        allocator.set_frame_slot(2);
        allocator.allocate(dev, dag, textures, {}, 3840, 2160);
        EXPECT_EQ(dev.created_textures, 3U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 2U);
        EXPECT_EQ(dev.destroyed_textures, 0U);

        // Resize 3 (4K -> 720p)
        allocator.on_surface_resize(dev);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 3U);
        EXPECT_EQ(dev.destroyed_textures, 0U);

        // Multiple redundant resizes should be no-ops on empty pool
        allocator.on_surface_resize(dev);
        allocator.on_surface_resize(dev);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 3U);
        EXPECT_EQ(dev.destroyed_textures, 0U);

        // Act: Advance to Slot 0 at 720p. Only Slot 0's retired texture should be destroyed!
        allocator.set_frame_slot(0);
        allocator.allocate(dev, dag, textures, {}, 1280, 720);
        EXPECT_EQ(dev.destroyed_textures, 1U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 2U);

        // Act: Advance to Slot 1 at 720p. Only Slot 1's retired texture should be destroyed!
        allocator.set_frame_slot(1);
        allocator.allocate(dev, dag, textures, {}, 1280, 720);
        EXPECT_EQ(dev.destroyed_textures, 2U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 1U);

        // Act: Advance to Slot 2 at 720p. Only Slot 2's retired texture should be destroyed!
        allocator.set_frame_slot(2);
        allocator.allocate(dev, dag, textures, {}, 1280, 720);
        EXPECT_EQ(dev.destroyed_textures, 3U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 0U);

        // Teardown: Clean release
        allocator.release_all(dev);
        EXPECT_EQ(dev.created_textures, dev.destroyed_textures);
        EXPECT_EQ(allocator.get_texture_pool_count(), 0U);
        EXPECT_EQ(allocator.get_retired_texture_pool_count(), 0U);
    }
} // namespace tempest::render_graph
