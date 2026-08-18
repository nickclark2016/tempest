#ifndef tempest_render_graph_context_hpp
#define tempest_render_graph_context_hpp

#include <tempest/api.hpp>
#include <tempest/int.hpp>
#include <tempest/render_graph/types.hpp>
#include <tempest/rhi.hpp>

namespace tempest::render_graph
{
    class render_graph;

    /// \brief Execution context passed to pass execution callbacks.
    /// Provides access to physical RHI handles, bindless descriptor indices, and GPU buffer device addresses.
    class TEMPEST_API pass_execution_context
    {
      public:
        explicit pass_execution_context(const render_graph* graph) noexcept : _graph{graph}
        {
        }

        [[nodiscard]] auto get_texture(rg_texture_id tex) const -> rhi::texture_handle;
        [[nodiscard]] auto get_texture_view(rg_texture_id tex) const -> rhi::texture_view_handle;
        [[nodiscard]] auto get_buffer(rg_buffer_id buf) const -> rhi::buffer_handle;
        [[nodiscard]] auto get_buffer_device_address(rg_buffer_id buf) const -> uint64_t;
        [[nodiscard]] auto get_texture_descriptor(rg_texture_id tex) const -> uint32_t;
        [[nodiscard]] auto get_storage_texture_descriptor(rg_texture_id tex) const -> uint32_t;
        [[nodiscard]] auto get_resolved_size(rg_texture_id tex) const -> resolved_size;

      private:
        const render_graph* _graph{nullptr};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_context_hpp
