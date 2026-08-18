#ifndef tempest_render_graph_temporal_texture_hpp
#define tempest_render_graph_temporal_texture_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>

namespace tempest::render_graph
{
    /// \brief Persistent, double/multi-buffered GPU texture container for temporal accumulation and history.
    ///
    /// Manages K physical texture allocations (where K = history_count + 1) in a ring buffer.
    /// Resources persist across render_graph::reset() calls and are automatically swapped upon execution.
    class TEMPEST_API temporal_texture
    {
      public:
        temporal_texture() = default;
        ~temporal_texture() = default;

        temporal_texture(const temporal_texture&) = delete;
        temporal_texture& operator=(const temporal_texture&) = delete;
        temporal_texture(temporal_texture&&) noexcept = default;
        temporal_texture& operator=(temporal_texture&&) noexcept = default;

        /// \brief Allocate K physical GPU textures and default views according to the descriptor.
        /// \param dev The RHI device used for GPU memory allocations.
        /// \param desc Descriptor specifying texture format, dimensions/scaling, and history depth.
        /// \param surface_width Current surface/window width for relative texture sizing.
        /// \param surface_height Current surface/window height for relative texture sizing.
        /// \return True on successful allocation of all physical slots.
        auto init(rhi::device& dev, const temporal_texture_desc& desc, uint32_t surface_width, uint32_t surface_height)
            -> bool;

        /// \brief Reallocate physical GPU textures on viewport/surface resize and invalidate history.
        auto on_resize(rhi::device& dev, uint32_t surface_width, uint32_t surface_height) -> void;

        /// \brief Destroy all allocated GPU texture and view resources.
        auto release(rhi::device& dev) -> void;

        /// \brief Logically invalidate all past history frames (e.g. on scene cuts or camera teleports).
        auto invalidate() noexcept -> void
        {
            _valid_history_frames = 0;
        }

        /// \brief Query if history at a specific past frame delta is valid.
        /// \param frame_delta 1 = Frame N-1, 2 = Frame N-2, etc.
        [[nodiscard]] auto is_history_valid(uint32_t frame_delta = 1) const noexcept -> bool
        {
            return frame_delta > 0 && _valid_history_frames >= frame_delta;
        }

        /// \brief Query the number of consecutively valid history frames currently recorded.
        [[nodiscard]] auto get_valid_history_count() const noexcept -> uint32_t
        {
            return _valid_history_frames;
        }

        /// \brief Advance the ring buffer by 1 slot and increment valid history count.
        auto swap() noexcept -> void
        {
            if (_textures.empty())
            {
                return;
            }
            _current_slot = static_cast<uint32_t>((_current_slot + 1) % _textures.size());
            _valid_history_frames =
                tempest::min(_valid_history_frames + 1, static_cast<uint32_t>(_desc.history_count));
        }

        /// \brief Access physical texture handle for current write target (Frame N).
        [[nodiscard]] auto get_write_texture() const noexcept -> rhi::texture_handle
        {
            return _textures.empty() ? rhi::texture_handle{} : _textures[_current_slot];
        }

        /// \brief Access physical texture view handle for current write target (Frame N).
        [[nodiscard]] auto get_write_view() const noexcept -> rhi::texture_view_handle
        {
            return _views.empty() ? rhi::texture_view_handle{} : _views[_current_slot];
        }

        /// \brief Access physical texture handle for history from frame N - frame_delta.
        /// \param frame_delta 1 = Frame N-1, 2 = Frame N-2, etc.
        [[nodiscard]] auto get_history_texture(uint32_t frame_delta = 1) const noexcept -> rhi::texture_handle
        {
            if (_textures.empty())
            {
                return rhi::texture_handle{};
            }
            const auto count = static_cast<uint32_t>(_textures.size());
            const auto slot = (_current_slot + count - (frame_delta % count)) % count;
            return _textures[slot];
        }

        /// \brief Access physical texture view handle for history from frame N - frame_delta.
        /// \param frame_delta 1 = Frame N-1, 2 = Frame N-2, etc.
        [[nodiscard]] auto get_history_view(uint32_t frame_delta = 1) const noexcept -> rhi::texture_view_handle
        {
            if (_views.empty())
            {
                return rhi::texture_view_handle{};
            }
            const auto count = static_cast<uint32_t>(_views.size());
            const auto slot = (_current_slot + count - (frame_delta % count)) % count;
            return _views[slot];
        }

        /// \brief Access all physical textures in the ring buffer.
        [[nodiscard]] auto get_all_textures() const noexcept -> span<const rhi::texture_handle>
        {
            return span<const rhi::texture_handle>{_textures.data(), _textures.size()};
        }

        /// \brief Access all physical texture views in the ring buffer.
        [[nodiscard]] auto get_all_views() const noexcept -> span<const rhi::texture_view_handle>
        {
            return span<const rhi::texture_view_handle>{_views.data(), _views.size()};
        }

        /// \brief Access the configuration descriptor.
        [[nodiscard]] auto get_desc() const noexcept -> const temporal_texture_desc&
        {
            return _desc;
        }

        /// \brief Check if physical resources are currently allocated.
        [[nodiscard]] auto is_allocated() const noexcept -> bool
        {
            return !_textures.empty();
        }

      private:
        temporal_texture_desc _desc{};
        uint32_t _current_slot{0};
        uint32_t _valid_history_frames{0};

        inplace_vector<rhi::texture_handle, max_temporal_slots> _textures{};
        inplace_vector<rhi::texture_view_handle, max_temporal_slots> _views{};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_temporal_texture_hpp
