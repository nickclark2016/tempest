#ifndef tempest_render_graph_pass_builder_hpp
#define tempest_render_graph_pass_builder_hpp

#include <tempest/api.hpp>
#include <tempest/render_graph/dag.hpp>
#include <tempest/render_graph/types.hpp>

namespace tempest::render_graph
{
    class render_graph;

    /// \brief Builder object passed to a pass's setup callback to declare resource creations, accesses, attachments,
    /// and sinks.
    class TEMPEST_API pass_builder
    {
      public:
        pass_builder(render_graph* graph, pass_node* node) noexcept : _graph{graph}, _node{node}
        {
        }

        auto create_texture(const rg_texture_desc& desc) -> rg_texture_id;
        auto create_buffer(const rg_buffer_desc& desc) -> rg_buffer_id;

        auto import_texture(rhi::texture_handle handle, rhi::texture_view_handle view,
                            rhi::image_layout initial_layout = rhi::image_layout::undefined) -> rg_texture_id;
        auto import_texture(rhi::texture_handle handle, rhi::image_layout initial_layout = rhi::image_layout::undefined)
            -> rg_texture_id;
        auto import_buffer(rhi::buffer_handle handle) -> rg_buffer_id;

        auto read(rg_texture_id tex, enum_mask<rhi::pipeline_stage> stages, enum_mask<rhi::resource_access> access,
                  rhi::image_layout layout, const rg_subresource_range& subresource = {}) -> rg_texture_id;

        auto write(rg_texture_id tex, enum_mask<rhi::pipeline_stage> stages, enum_mask<rhi::resource_access> access,
                   rhi::image_layout layout, const rg_subresource_range& subresource = {}) -> rg_texture_id;

        auto read_write(rg_texture_id tex, enum_mask<rhi::pipeline_stage> stages,
                        enum_mask<rhi::resource_access> access, rhi::image_layout layout,
                        const rg_subresource_range& subresource = {}) -> rg_texture_id;

        auto set_color_attachment(uint32_t slot, const rg_color_attachment& attachment) -> rg_texture_id;
        auto set_depth_stencil_attachment(const rg_depth_stencil_attachment& attachment) -> rg_texture_id;

        auto use_temporal_texture(temporal_texture& tex, uint32_t requested_history_depth = 1) -> temporal_binding;
        auto set_temporal_color_attachment(uint32_t slot, const rg_temporal_color_attachment& attachment)
            -> rg_texture_id;
        void clear_temporal_texture(temporal_texture& tex, rhi::clear_color_value clear_value = {});

        auto read(rg_buffer_id buf, enum_mask<rhi::pipeline_stage> stages, enum_mask<rhi::resource_access> access,
                  uint64_t offset = 0, uint64_t size = buffer_access::whole_size) -> rg_buffer_id;

        auto write(rg_buffer_id buf, enum_mask<rhi::pipeline_stage> stages, enum_mask<rhi::resource_access> access,
                   uint64_t offset = 0, uint64_t size = buffer_access::whole_size) -> rg_buffer_id;

        auto read_write(rg_buffer_id buf, enum_mask<rhi::pipeline_stage> stages, enum_mask<rhi::resource_access> access,
                        uint64_t offset = 0, uint64_t size = buffer_access::whole_size) -> rg_buffer_id;

        void fallback(rg_texture_id produced, rg_texture_id fallback_source);
        void fallback(rg_buffer_id produced, rg_buffer_id fallback_source);

        auto passthrough(rg_texture_id input) -> rg_texture_id;
        auto passthrough(rg_buffer_id input) -> rg_buffer_id;

        void mark_sink() noexcept;
        void set_execution_queue(queue_type queue) noexcept;
        void set_enable_condition(function<bool()> condition);
        auto enable_pipeline_statistics(enum_mask<rhi::pipeline_statistic_flags> stats) -> pass_builder&;

      private:
        render_graph* _graph{nullptr};
        pass_node* _node{nullptr};
    };
} // namespace tempest::render_graph

#endif // tempest_render_graph_pass_builder_hpp
