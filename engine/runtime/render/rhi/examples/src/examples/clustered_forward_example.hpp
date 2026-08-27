#ifndef tempest_rhi_examples_clustered_forward_example_hpp
#define tempest_rhi_examples_clustered_forward_example_hpp

#include "../example.hpp"

#include <tempest/archetype.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/texture.hpp>
#include <tempest/vector.hpp>
#include <tempest/vertex.hpp>

namespace tempest::rhi::examples
{
    class clustered_forward_example final : public example
    {
      public:
        [[nodiscard]] static auto create() -> unique_ptr<example>
        {
            return make_unique<clustered_forward_example>();
        }

        [[nodiscard]] auto init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool override;
        auto render(const frame_render_info& info) -> void override;
        auto on_resize(rhi::device& dev, rhi::render_surface_format surface_format, uint32_t width,
                       uint32_t height) -> void override;
        auto shutdown(rhi::device& dev) -> void override;

      private:
        stdout_log_sink _log_sink{};
        logger _logger{_log_sink};
        event::event_registry _events{};
        ecs::archetype_registry _registry{_events};
        core::mesh_registry _meshes{};
        core::material_registry _materials{};
        core::texture_registry _textures{};

        unique_ptr<render_system::renderer> _renderer;
        ecs::entity _camera_entity{ecs::tombstone};
        ecs::entity _root_entity{ecs::tombstone};
        vector<ecs::entity> _light_entities{};
        float _time{0.0F};
    };
} // namespace tempest::rhi::examples

#endif // tempest_rhi_examples_clustered_forward_example_hpp
