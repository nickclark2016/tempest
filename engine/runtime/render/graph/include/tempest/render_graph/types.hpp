#ifndef tempest_render_graph_types_hpp
#define tempest_render_graph_types_hpp

#include <tempest/api.hpp>
#include <tempest/cstring_view.hpp>
#include <tempest/enum.hpp>
#include <tempest/hash.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/int.hpp>
#include <tempest/limits.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_graph
{
    inline constexpr uint64_t null_device_address = 0ULL;
    inline constexpr uint32_t invalid_descriptor_index = ~0U;

    /// \brief Static Single Assignment (SSA) versioned handle to a render graph texture resource.
    struct rg_texture_id
    {
        static constexpr uint32_t invalid_id = numeric_limits<uint32_t>::max();
        static constexpr uint16_t invalid_version = numeric_limits<uint16_t>::max();

        uint32_t id = invalid_id;
        uint16_t version = 0;

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return id != invalid_id;
        }

        [[nodiscard]] explicit constexpr operator bool() const noexcept
        {
            return is_valid();
        }

        [[nodiscard]] constexpr auto operator==(const rg_texture_id& other) const noexcept -> bool
        {
            return id == other.id && version == other.version;
        }

        [[nodiscard]] constexpr auto operator!=(const rg_texture_id& other) const noexcept -> bool
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr auto next_version() const noexcept -> rg_texture_id
        {
            return rg_texture_id{
                .id = id,
                .version = static_cast<uint16_t>(version + 1),
            };
        }
    };

    /// \brief Static Single Assignment (SSA) versioned handle to a render graph buffer resource.
    struct rg_buffer_id
    {
        static constexpr uint32_t invalid_id = numeric_limits<uint32_t>::max();
        static constexpr uint16_t invalid_version = numeric_limits<uint16_t>::max();

        uint32_t id = invalid_id;
        uint16_t version = 0;

        [[nodiscard]] constexpr auto is_valid() const noexcept -> bool
        {
            return id != invalid_id;
        }

        [[nodiscard]] explicit constexpr operator bool() const noexcept
        {
            return is_valid();
        }

        [[nodiscard]] constexpr auto operator==(const rg_buffer_id& other) const noexcept -> bool
        {
            return id == other.id && version == other.version;
        }

        [[nodiscard]] constexpr auto operator!=(const rg_buffer_id& other) const noexcept -> bool
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr auto next_version() const noexcept -> rg_buffer_id
        {
            return rg_buffer_id{
                .id = id,
                .version = static_cast<uint16_t>(version + 1),
            };
        }
    };

    /// \brief The queue family / execution port type on which a pass or work runs.
    enum class queue_type : uint8_t
    {
        graphics,
        async_compute,
        async_transfer,
    };

    /// \brief Sizing strategy for render graph textures.
    enum class size_mode : uint8_t
    {
        absolute,         ///< Fixed pixel dimensions.
        surface_relative, ///< Scale factors relative to the output render surface / swapchain.
        custom_relative,  ///< Scale factors relative to another texture.
    };

    struct resolved_size
    {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };

    /// \brief Size specification for render graph textures.
    struct rg_texture_size
    {
        size_mode mode = size_mode::surface_relative;
        float scale_x = 1.0F;
        float scale_y = 1.0F;
        uint32_t absolute_width = 0;
        uint32_t absolute_height = 0;
        uint32_t depth = 1;
        rg_texture_id relative_source{};

        static constexpr auto absolute(uint32_t width, uint32_t height, uint32_t depth = 1) noexcept -> rg_texture_size
        {
            return rg_texture_size{
                .mode = size_mode::absolute,
                .scale_x = 1.0F,
                .scale_y = 1.0F,
                .absolute_width = width,
                .absolute_height = height,
                .depth = depth,
                .relative_source = {},
            };
        }

        static constexpr auto surface_relative(float scale_x = 1.0F, float scale_y = 1.0F,
                                               uint32_t depth = 1) noexcept -> rg_texture_size
        {
            return rg_texture_size{
                .mode = size_mode::surface_relative,
                .scale_x = scale_x,
                .scale_y = scale_y,
                .absolute_width = 0,
                .absolute_height = 0,
                .depth = depth,
                .relative_source = {},
            };
        }

        static constexpr auto custom_relative(rg_texture_id source, float scale_x = 1.0F, float scale_y = 1.0F,
                                              uint32_t depth = 1) noexcept -> rg_texture_size
        {
            return rg_texture_size{
                .mode = size_mode::custom_relative,
                .scale_x = scale_x,
                .scale_y = scale_y,
                .absolute_width = 0,
                .absolute_height = 0,
                .depth = depth,
                .relative_source = source,
            };
        }

        [[nodiscard]] auto evaluate(uint32_t surface_width, uint32_t surface_height) const noexcept
            -> resolved_size
        {
            if (mode == size_mode::absolute)
            {
                return resolved_size{
                    .width = absolute_width,
                    .height = absolute_height,
                    .depth = depth,
                };
            }

            const auto w = static_cast<uint32_t>(static_cast<float>(surface_width) * scale_x);
            const auto h = static_cast<uint32_t>(static_cast<float>(surface_height) * scale_y);

            return resolved_size{
                .width = w > 0 ? w : 1U,
                .height = h > 0 ? h : 1U,
                .depth = depth > 0 ? depth : 1U,
            };
        }
    };

    /// \brief Specification for creating transient or pooled textures in the render graph.
    struct rg_texture_desc
    {
        rg_texture_size size = rg_texture_size::surface_relative(1.0F, 1.0F);
        rhi::data_format format = rhi::data_format::rgba8_unorm;
        rhi::memory_usage memory_usage = rhi::memory_usage::device_only;
        enum_mask<rhi::texture_usage> usage = rhi::texture_usage::sampled | rhi::texture_usage::storage |
                                              rhi::texture_usage::color_attachment | rhi::texture_usage::transfer_src |
                                              rhi::texture_usage::transfer_dst;
        uint32_t mip_levels = 1;
        uint32_t array_layers = 1;
        cstring_view name;
    };

    /// \brief Specification for creating transient or pooled buffers in the render graph.
    struct rg_buffer_desc
    {
        uint64_t size = 0;
        rhi::memory_usage memory_usage = rhi::memory_usage::device_only;
        enum_mask<rhi::buffer_usage> usage = rhi::buffer_usage::storage_buffer | rhi::buffer_usage::device_address;
        cstring_view name;
    };

    /// \brief Subresource selection for targeted barrier or copy operations.
    struct rg_subresource_range
    {
        uint32_t base_mip_level = 0;
        uint32_t mip_level_count = ~0U;
        uint32_t base_array_layer = 0;
        uint32_t array_layer_count = ~0U;
    };

    /// \brief Specification for a color attachment in a render graph pass.
    struct rg_color_attachment
    {
        rg_texture_id texture;
        rhi::load_op load_op = rhi::load_op::clear;
        rhi::store_op store_op = rhi::store_op::store;
        rhi::clear_color_value clear_value{0.0F, 0.0F, 0.0F, 1.0F};
        rg_subresource_range subresource{};
    };

    /// \brief Specification for a depth/stencil attachment in a render graph pass.
    struct rg_depth_stencil_attachment
    {
        rg_texture_id texture;
        rhi::load_op depth_load_op = rhi::load_op::clear;
        rhi::store_op depth_store_op = rhi::store_op::store;
        rhi::load_op stencil_load_op = rhi::load_op::dont_care;
        rhi::store_op stencil_store_op = rhi::store_op::dont_care;
        rhi::clear_depth_stencil_value clear_value{1.0F, 0};
        rg_subresource_range subresource{};
    };

    /// \brief Maximum physical texture slots in a temporal resource ring buffer.
    inline constexpr size_t max_temporal_slots = 4;

    /// \brief Maximum past history frames that can be requested (max_temporal_slots - 1).
    inline constexpr size_t max_history_frames = max_temporal_slots - 1;

    /// \brief Descriptor for creating a temporal texture with configurable history depth.
    struct temporal_texture_desc
    {
        rg_texture_desc desc;
        uint32_t history_count = 1; ///< Number of past frames to preserve (1..3).
    };

    /// \brief Temporal binding providing read handles to past history frames and a write target for current frame.
    struct temporal_binding
    {
        inplace_vector<rg_texture_id, max_history_frames> history_reads{}; ///< [0] = Frame N-1, [1] = Frame N-2, etc.
        rg_texture_id target_write{};                                      ///< Current frame (N) write destination.

        [[nodiscard]] constexpr auto has_history(uint32_t delta = 1) const noexcept -> bool
        {
            return delta > 0 && delta <= history_reads.size() && history_reads[delta - 1].is_valid();
        }

        [[nodiscard]] constexpr auto get_history(uint32_t delta = 1) const noexcept -> rg_texture_id
        {
            return has_history(delta) ? history_reads[delta - 1] : rg_texture_id{};
        }
    };

    class temporal_texture;

    /// \brief Color attachment description referencing a persistent temporal texture.
    struct rg_temporal_color_attachment
    {
        temporal_texture& texture;
        rhi::load_op load_op = rhi::load_op::clear;
        rhi::store_op store_op = rhi::store_op::store;
        rhi::clear_color_value clear_value{0.0F, 0.0F, 0.0F, 1.0F};
        rg_subresource_range subresource{};
    };
} // namespace tempest::render_graph

namespace tempest
{
    template <>
    struct hash<render_graph::rg_texture_id>
    {
        auto operator()(render_graph::rg_texture_id key) const noexcept -> size_t
        {
            auto h = static_cast<uint64_t>(key.id) | (static_cast<uint64_t>(key.version) << 32);
            return hash<uint64_t>{}(h);
        }
    };

    template <>
    struct hash<render_graph::rg_buffer_id>
    {
        auto operator()(render_graph::rg_buffer_id key) const noexcept -> size_t
        {
            auto h = static_cast<uint64_t>(key.id) | (static_cast<uint64_t>(key.version) << 32);
            return hash<uint64_t>{}(h);
        }
    };
} // namespace tempest

#endif // tempest_render_graph_types_hpp
