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

        uint32_t vertex_data_buffer_size;
        uint32_t mesh_data_buffer_size;
        uint32_t material_data_buffer_size;
        uint32_t staging_buffer_size_per_frame;
        uint32_t max_lights;

        float max_anisotropy;
    };

    struct pbr_frame_graph_inputs
    {
        ecs::archetype_registry* entity_registry = nullptr;
    };

    class pbr_frame_graph
    {
      public:
        pbr_frame_graph(rhi::device& device, pbr_frame_graph_config cfg, pbr_frame_graph_inputs inputs);

        pbr_frame_graph(const pbr_frame_graph&) = delete;
        pbr_frame_graph(pbr_frame_graph&&) noexcept = delete;

        ~pbr_frame_graph();

        pbr_frame_graph& operator=(const pbr_frame_graph&) = delete;
        pbr_frame_graph& operator=(pbr_frame_graph&&) noexcept = delete;

        optional<graph_builder&> get_builder() noexcept;

        void compile(queue_configuration cfg);

        void execute();

      private:
        rhi::device* _device;
        pbr_frame_graph_config _cfg;
        pbr_frame_graph_inputs _inputs;

        optional<graph_builder> _builder;
        optional<graph_executor> _executor;

        graph_resource_handle<rhi::rhi_handle_type::buffer> vertex_pull_buffer;
        graph_resource_handle<rhi::rhi_handle_type::buffer> mesh_buffer;
        graph_resource_handle<rhi::rhi_handle_type::buffer> material_buffer;
        graph_resource_handle<rhi::rhi_handle_type::buffer> light_buffer;

        rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_sampler;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> linear_with_aniso_sampler;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_sampler;
        rhi::typed_rhi_handle<rhi::rhi_handle_type::sampler> point_with_aniso_sampler;

        void _initialize();
    };
} // namespace tempest::graphics

#endif // tempest_graphics_pbr_frame_graph_hpp
