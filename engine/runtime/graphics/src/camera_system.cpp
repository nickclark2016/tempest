#include <tempest/camera_system.hpp>

#include <tempest/ecs_events.hpp>
#include <tempest/mat4.hpp>
#include <tempest/quat.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vector.hpp>

namespace tempest::graphics
{
    camera_system::camera_system(ecs::registry& registry, [[maybe_unused]] event::event_registry& events)
        : _registry(&registry)
    {
    }

    camera_system::camera_system(ecs::registry& registry) : camera_system(registry, registry.event_registry())
    {
    }

    auto camera_system::get_active_camera_entity() const -> tempest::optional<ecs::entity>
    {
        if (_registry == nullptr)
        {
            return tempest::nullopt;
        }

        if (_active_camera_entity.has_value())
        {
            const auto entity = *_active_camera_entity;
            if (_registry->is_valid(entity) && _registry->has<camera_component>(entity) &&
                _registry->has<ecs::transform_component>(entity))
            {
                const auto* cam = _registry->try_get<camera_component>(entity);
                if (cam != nullptr && cam->is_active)
                {
                    return entity;
                }
            }
            _active_camera_entity = tempest::nullopt;
        }

        auto fallback = tempest::optional<ecs::entity>();
        _registry->each(
            [&](const ecs::self_component& self, const camera_component& cam, const ecs::transform_component&) {
                if (!fallback.has_value() && cam.is_active && cam.target == camera_target_type::viewport)
                {
                    fallback = self.entity;
                }
            });

        return fallback;
    }

    auto camera_system::get_active_camera() const -> tempest::optional<render_camera>
    {
        if (_registry == nullptr)
        {
            return tempest::nullopt;
        }

        const auto active_entity_opt = get_active_camera_entity();
        if (!active_entity_opt.has_value())
        {
            return tempest::nullopt;
        }

        const auto entity = active_entity_opt.value();
        const auto* const cam_comp = _registry->try_get<camera_component>(entity);
        const auto* const tx_comp = _registry->try_get<ecs::transform_component>(entity);

        if (cam_comp == nullptr || tx_comp == nullptr)
        {
            return tempest::nullopt;
        }

        const auto pos = tx_comp->position();
        const auto quat_rot = math::quat(tx_comp->rotation());
        const auto f = math::extract_forward(quat_rot);
        const auto u = math::extract_up(quat_rot);

        const auto proj = math::perspective(cam_comp->aspect_ratio, cam_comp->vertical_fov, cam_comp->near_plane);
        const auto view = math::look_at(pos, pos + f, u);
        const auto inv_proj = math::inverse(proj);
        const auto inv_view = math::inverse(view);
        const auto eye_pos = math::vec4<float>(pos.x, pos.y, pos.z, 1.0F);

        return render_camera{
            .proj = proj,
            .inv_proj = inv_proj,
            .view = view,
            .inv_view = inv_view,
            .eye_position = eye_pos,
        };
    }

    auto camera_system::set_active_camera(tempest::optional<ecs::entity> camera_entity) -> void
    {
        _active_camera_entity = camera_entity;
    }

    auto camera_system::clear_active_camera() -> void
    {
        _active_camera_entity = tempest::nullopt;
    }
} // namespace tempest::graphics
