#ifndef tempest_graphics_pbr_frame_graph_hpp
#define tempest_graphics_pbr_frame_graph_hpp

#include <tempest/archetype.hpp>
#include <tempest/frame_graph.hpp>
#include <tempest/int.hpp>
#include <tempest/rhi.hpp>
#include <tempest/rhi_types.hpp>

namespace tempest::graphics
{
    struct pbr_frame_graph_config
    {
        uint32_t render_target_width;
        uint32_t render_target_height;
        uint32_t shadow_map_width;
        uint32_t shadow_map_height;
        rhi::image_format hdr_color_format;
        rhi::image_format depth_format;
        rhi::image_format tonemapped_color_format;
    };

    struct pbr_frame_graph_inputs
    {
        ecs::archetype_registry* entity_registry = nullptr;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> vertex_pull_buffer;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> index_buffer;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> mesh_buffer;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::buffer> material_buffer;
    };

    struct pbr_frame_graph_handles
    {
        graph_resource_handle<rhi::rhi_handle_type::image> hdr_color;
        graph_resource_handle<rhi::rhi_handle_type::image> depth;
        graph_resource_handle<rhi::rhi_handle_type::image> tonemapped_color;
    };

    pbr_frame_graph_handles create_pbr_frame_graph(graph_builder& graph_builder, rhi::device* device,
                                                   pbr_frame_graph_config cfg, pbr_frame_graph_inputs inputs);
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_frame_graph_hpp
