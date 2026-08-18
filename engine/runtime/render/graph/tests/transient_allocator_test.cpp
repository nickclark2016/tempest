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

            auto wait_idle() -> void override {}
            auto wait_for_sync([[maybe_unused]] rhi::host_sync_point sync_point) -> void override {}

            [[nodiscard]] auto is_ray_tracing_supported() const -> bool override { return false; }
            [[nodiscard]] auto is_mesh_shading_supported() const -> bool override { return false; }
            [[nodiscard]] auto is_ray_query_supported() const -> bool override { return false; }

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
                ++created_buffers;
                return rhi::buffer_handle{.handle = next_handle++};
            }

            [[nodiscard]] auto create_texture([[maybe_unused]] const rhi::texture_desc& desc) -> rhi::texture_handle override
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

            [[nodiscard]] auto create_sampler([[maybe_unused]] const rhi::sampler_desc& desc) -> rhi::sampler_handle override
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

            auto destroy_sampler([[maybe_unused]] rhi::sampler_handle sampler) -> void override {}
            auto destroy_graphics_pipeline([[maybe_unused]] rhi::graphics_pipeline_handle pipeline) -> void override {}
            auto destroy_compute_pipeline([[maybe_unused]] rhi::compute_pipeline_handle pipeline) -> void override {}
            auto destroy_event([[maybe_unused]] rhi::event_handle event) -> void override {}
            auto destroy_semaphore([[maybe_unused]] rhi::semaphore_handle semaphore) -> void override {}

            [[nodiscard]] auto allocate_descriptor([[maybe_unused]] rhi::descriptor_type type) -> rhi::descriptor_handle override
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
                                          [[maybe_unused]] rhi::sampler_handle sampler) -> void override {}
            auto write_sampled_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                                [[maybe_unused]] rhi::texture_view_handle view,
                                                [[maybe_unused]] rhi::image_layout layout) -> void override {}
            auto write_storage_image_descriptor([[maybe_unused]] rhi::descriptor_handle slot,
                                                [[maybe_unused]] rhi::texture_view_handle view,
                                                [[maybe_unused]] rhi::image_layout layout) -> void override {}
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
                .desc = rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "TexA",
                },
            },
            registered_texture{
                .id = 1,
                .desc = rg_texture_desc{
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
                .desc = rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "TexA",
                },
            },
            registered_texture{
                .id = 1,
                .desc = rg_texture_desc{
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
                .desc = rg_texture_desc{
                    .size = rg_texture_size::surface_relative(1.0F, 1.0F),
                    .format = rhi::data_format::rgba8_unorm,
                    .name = "SurfaceRelativeTex",
                },
            },
            registered_texture{
                .id = 1,
                .desc = rg_texture_desc{
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
        // Only surface-relative texture destroyed
        EXPECT_EQ(dev.destroyed_textures, 1U);
        EXPECT_EQ(allocator.get_texture_pool_count(), 1U); // FixedTex remains!

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
} // namespace tempest::render_graph
