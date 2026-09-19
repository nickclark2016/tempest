#ifndef tempest_render_graph_flight_texture_hpp
#define tempest_render_graph_flight_texture_hpp

#include <tempest/algorithm.hpp>
#include <tempest/api.hpp>
#include <tempest/inplace_vector.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>
#include <tempest/span.hpp>
#include <tempest/utility.hpp>

namespace tempest::render_graph
{
    /// \brief Container holding extracted GPU resources from a flight texture for safe retirement handover.
    struct flight_resources
    {
        inplace_vector<rhi::texture_handle, max_flight_slots> textures{};
        inplace_vector<rhi::texture_view_handle, max_flight_slots> views{};
        inplace_vector<rhi::descriptor_handle, max_flight_slots> sampled_descriptors{};
    };

    /// \brief Persistent GPU texture container multi-buffered across frames in flight.
    ///
    /// Manages K physical texture allocations (matching frames in flight) in an explicit slot ring.
    /// Resources persist across frame graph compilation/execution and are indexed directly by flight slot.
    class TEMPEST_API flight_texture
    {
      public:
        flight_texture() = default;
        ~flight_texture() = default;

        flight_texture(const flight_texture&) = delete;
        flight_texture& operator=(const flight_texture&) = delete;
        flight_texture(flight_texture&&) noexcept = default;
        flight_texture& operator=(flight_texture&&) noexcept = default;

        /// \brief Allocate K physical GPU textures and views according to the descriptor.
        /// \param dev The RHI device used for GPU memory allocations.
        /// \param desc Descriptor specifying texture format, dimensions/scaling, and flight slot count.
        /// \param surface_width Current surface/window width for relative texture sizing.
        /// \param surface_height Current surface/window height for relative texture sizing.
        /// \return True on successful allocation of all physical slots.
        auto init(rhi::device& dev, const flight_texture_desc& desc, uint32_t surface_width, uint32_t surface_height)
            -> bool;

        /// \brief Reallocate physical GPU textures on viewport/surface resize.
        auto on_resize(rhi::device& dev, uint32_t surface_width, uint32_t surface_height) -> void;

        /// \brief Destroy all allocated GPU texture and view resources.
        auto release(rhi::device& dev) -> void;

        /// \brief Extract allocated GPU resources for deferred deletion handover, leaving container unallocated.
        auto extract_resources() noexcept -> flight_resources
        {
            auto res = flight_resources{
                .textures = tempest::move(_textures),
                .views = tempest::move(_views),
                .sampled_descriptors = tempest::move(_sampled_descriptors),
            };
            _textures.clear();
            _views.clear();
            _sampled_descriptors.clear();
            return res;
        }

        /// \brief Check if physical resources are currently allocated.
        [[nodiscard]] auto is_allocated() const noexcept -> bool
        {
            return !_textures.empty();
        }

        /// \brief Access the number of allocated physical flight slots.
        [[nodiscard]] auto get_slot_count() const noexcept -> uint32_t
        {
            return static_cast<uint32_t>(_textures.size());
        }

        /// \brief Access physical texture handle for a specific flight slot.
        [[nodiscard]] auto get_texture(uint32_t slot) const noexcept -> rhi::texture_handle
        {
            if (_textures.empty())
            {
                return rhi::texture_handle{};
            }
            return _textures[slot % _textures.size()];
        }

        /// \brief Access physical texture view handle for a specific flight slot.
        [[nodiscard]] auto get_view(uint32_t slot) const noexcept -> rhi::texture_view_handle
        {
            if (_views.empty())
            {
                return rhi::texture_view_handle{};
            }
            return _views[slot % _views.size()];
        }

        /// \brief Access sampled image descriptor handle for a specific flight slot.
        [[nodiscard]] auto get_sampled_descriptor(uint32_t slot) const noexcept -> rhi::descriptor_handle
        {
            if (_sampled_descriptors.empty())
            {
                return rhi::descriptor_handle{};
            }
            return _sampled_descriptors[slot % _sampled_descriptors.size()];
        }

        /// \brief Access all physical textures in the flight container.
        [[nodiscard]] auto get_all_textures() const noexcept -> span<const rhi::texture_handle>
        {
            return span<const rhi::texture_handle>{_textures.data(), _textures.size()};
        }

        /// \brief Access all physical texture views in the flight container.
        [[nodiscard]] auto get_all_views() const noexcept -> span<const rhi::texture_view_handle>
        {
            return span<const rhi::texture_view_handle>{_views.data(), _views.size()};
        }

        /// \brief Access all sampled image descriptors in the flight container.
        [[nodiscard]] auto get_all_sampled_descriptors() const noexcept -> span<const rhi::descriptor_handle>
        {
            return span<const rhi::descriptor_handle>{_sampled_descriptors.data(), _sampled_descriptors.size()};
        }

        /// \brief Access the configuration descriptor.
        [[nodiscard]] auto get_desc() const noexcept -> const flight_texture_desc&
        {
            return _desc;
        }

      private:
        flight_texture_desc _desc{};

        inplace_vector<rhi::texture_handle, max_flight_slots> _textures{};
        inplace_vector<rhi::texture_view_handle, max_flight_slots> _views{};
        inplace_vector<rhi::descriptor_handle, max_flight_slots> _sampled_descriptors{};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_flight_texture_hpp
