#ifndef tempest_graphics_renderer_hpp
#define tempest_graphics_renderer_hpp

#include <tempest/functional.hpp>
#include <tempest/memory.hpp>
#include <tempest/pbr_frame_graph.hpp>
#include <tempest/rhi.hpp>
#include <tempest/vector.hpp>

namespace tempest::graphics
{
    class renderer;

    class renderer
    {
      public:
        class builder
        {
          public:
            builder() = default;
            builder(const builder&) = delete;
            builder(builder&&) noexcept = delete;
            ~builder() = default;

            builder& operator=(const builder&) = delete;
            builder& operator=(builder&&) noexcept = delete;

            builder& set_pbr_frame_graph_config(pbr_frame_graph_config cfg);
            builder& set_pbr_frame_graph_inputs(pbr_frame_graph_inputs inputs);
            builder& add_pbr_frame_graph_customization_callback(function<void(pbr_frame_graph&)> callback);

            renderer build();

          private:
            pbr_frame_graph_config _pbr_cfg = {};
            pbr_frame_graph_inputs _pbr_inputs = {};
            vector<function<void(pbr_frame_graph&)>> _pbr_customization_callbacks = {};
        };

        unique_ptr<rhi::window_surface> create_window(const rhi::window_surface_desc& desc);
        void upload_objects_sync(span<const ecs::archetype_entity> entities, const core::mesh_registry& meshes,
                                 const core::texture_registry& textures, const core::material_registry& materials);

        void finalize_graph();

        void render();

      private:
        explicit renderer(unique_ptr<rhi::instance> instance, rhi::device& device, unique_ptr<pbr_frame_graph> graph);

        unique_ptr<rhi::instance> _instance;
        rhi::device* _device;
        unique_ptr<pbr_frame_graph> _graph;
    };
} // namespace tempest::graphics

#endif // tempest_graphics_renderer_hpp
