#ifndef tempest_rhi_examples_render_system_example_hpp
#define tempest_rhi_examples_render_system_example_hpp

#include "../example.hpp"

#include <tempest/archetype.hpp>
#include <tempest/asset_database.hpp>
#include <tempest/asset_type_registry.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/logger.hpp>
#include <tempest/material.hpp>
#include <tempest/render_system/camera_system.hpp>
#include <tempest/render_system/renderer.hpp>
#include <tempest/texture.hpp>
#include <tempest/vertex.hpp>

namespace tempest::rhi::examples
{
    enum class scene_model
    {
        sponza,
        chess,
    };

    class render_system_example final : public example
    {
      public:
        [[nodiscard]] static auto create() -> unique_ptr<example>
        {
            auto ex = make_unique<render_system_example>();
            ex->set_model(_s_default_model);
            return ex;
        }

        [[nodiscard]] static auto create_chess() -> unique_ptr<example>
        {
            auto ex = make_unique<render_system_example>();
            ex->set_model(scene_model::chess);
            return ex;
        }

        static void set_default_model(scene_model model) noexcept
        {
            _s_default_model = model;
        }

        void set_model(scene_model model) noexcept
        {
            _model = model;
        }

        [[nodiscard]] auto init(rhi::device& dev, rhi::render_surface_format surface_format) -> bool override;
        auto render(const frame_render_info& info) -> void override;
        auto on_resize(rhi::device& dev, rhi::render_surface_format surface_format, uint32_t width, uint32_t height)
            -> void override;
        auto shutdown(rhi::device& dev) -> void override;

      private:
        static inline scene_model _s_default_model{scene_model::sponza};

        stdout_log_sink _log_sink{};
        logger _logger{_log_sink};
        event::event_registry _events{};
        ecs::archetype_registry _registry{_events};
        core::mesh_registry _meshes{};
        core::material_registry _materials{};
        core::texture_registry _textures{};
        assets::asset_type_registry _asset_types{};
        assets::asset_database _asset_db{&_asset_types};

        unique_ptr<render_system::renderer> _renderer;
        ecs::entity _camera_entity{ecs::tombstone};
        ecs::entity _root_entity{ecs::tombstone};
        scene_model _model{scene_model::sponza};
        float _time{0.0F};
    };
} // namespace tempest::rhi::examples

#endif // tempest_rhi_examples_render_system_example_hpp
