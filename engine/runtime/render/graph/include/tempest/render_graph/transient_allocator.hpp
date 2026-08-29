#ifndef tempest_render_graph_transient_allocator_hpp
#define tempest_render_graph_transient_allocator_hpp

#include <tempest/api.hpp>
#include <tempest/flat_unordered_map.hpp>
#include <tempest/render_graph/dag.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_graph
{
    struct transient_pool_config
    {
        uint32_t max_unused_cycles{2};
        bool evict_mismatched_dimensions{true};
    };

    struct pooled_texture
    {
        rhi::texture_handle handle{};
        rhi::texture_view_handle view{};
        rhi::descriptor_handle sampled_descriptor{};
        rhi::descriptor_handle storage_descriptor{};
        rhi::texture_desc desc{};
        bool is_surface_relative = false;
        bool in_use_this_frame = false;
        uint32_t last_pass_used = 0;
        uint32_t flight_slot = 0;
        uint32_t unused_cycles = 0;
    };

    struct pooled_buffer
    {
        rhi::buffer_handle handle{};
        uint64_t device_address = 0;
        rhi::buffer_desc desc{};
        bool in_use_this_frame = false;
        uint32_t last_pass_used = 0;
        uint32_t flight_slot = 0;
        uint32_t unused_cycles = 0;
    };

    struct physical_texture_allocation
    {
        rhi::texture_handle handle{};
        rhi::texture_view_handle default_view{};
        rhi::descriptor_handle sampled_descriptor{};
        rhi::descriptor_handle storage_descriptor{};
        resolved_size size{};
        rhi::data_format format = rhi::data_format::unknown;
    };

    struct physical_buffer_allocation
    {
        rhi::buffer_handle handle{};
        uint64_t device_address = 0;
        uint64_t size = 0;
    };

    /// \brief Manages physical GPU resource pooling, non-overlapping lifetime recycling, and dynamic resolution
    /// invalidation.
    class TEMPEST_API transient_allocator
    {
      public:
        transient_allocator() = default;
        ~transient_allocator() = default;

        transient_allocator(const transient_allocator&) = delete;
        transient_allocator& operator=(const transient_allocator&) = delete;
        transient_allocator(transient_allocator&&) noexcept = default;
        transient_allocator& operator=(transient_allocator&&) noexcept = default;

        void set_frames_in_flight(uint32_t count) noexcept
        {
            _frames_in_flight = (count > 0) ? count : 1;
        }

        void set_frame_slot(uint32_t slot) noexcept
        {
            _frame_slot = (_frames_in_flight > 0) ? (slot % _frames_in_flight) : 0;
        }

        void advance_frame() noexcept
        {
            if (_frames_in_flight > 0)
            {
                _frame_slot = (_frame_slot + 1) % _frames_in_flight;
            }
        }

        [[nodiscard]] auto get_frame_slot() const noexcept -> uint32_t
        {
            return _frame_slot;
        }

        [[nodiscard]] auto get_frames_in_flight() const noexcept -> uint32_t
        {
            return _frames_in_flight;
        }

        /// \brief Allocate and recycle physical resources based on compiled DAG lifetime intervals.
        void allocate(rhi::device& dev, const compiled_dag& dag, span<const registered_texture> textures,
                      span<const registered_buffer> buffers, uint32_t surface_width, uint32_t surface_height);

        /// \brief Invalidate and destroy all pooled GPU resources.
        void release_all(rhi::device& dev);

        /// \brief Release only surface-relative textures when the display/swapchain surface resizes.
        void on_surface_resize(rhi::device& dev);

        [[nodiscard]] auto get_texture(uint32_t texture_id) const noexcept -> const physical_texture_allocation*;
        [[nodiscard]] auto get_buffer(uint32_t buffer_id) const noexcept -> const physical_buffer_allocation*;

        void set_pool_config(const transient_pool_config& cfg) noexcept
        {
            _pool_config = cfg;
        }

        [[nodiscard]] auto get_pool_config() const noexcept -> const transient_pool_config&
        {
            return _pool_config;
        }

        [[nodiscard]] auto get_texture_pool_count() const noexcept -> size_t
        {
            return _texture_pool.size();
        }

        [[nodiscard]] auto get_buffer_pool_count() const noexcept -> size_t
        {
            return _buffer_pool.size();
        }

      private:
        uint32_t _frames_in_flight{1};
        uint32_t _frame_slot{0};
        transient_pool_config _pool_config{};

        vector<pooled_texture> _texture_pool;
        vector<pooled_buffer> _buffer_pool;

        flat_unordered_map<uint32_t, physical_texture_allocation> _active_textures;
        flat_unordered_map<uint32_t, physical_buffer_allocation> _active_buffers;
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_transient_allocator_hpp
