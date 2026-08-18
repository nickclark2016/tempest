#include <tempest/render_system/camera_system.hpp>

#include <tempest/ecs_events.hpp>
#include <tempest/mat4.hpp>
#include <tempest/quat.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transformations.hpp>
#include <tempest/vector.hpp>

namespace tempest::render_system
{
    camera_system::camera_system(ecs::registry& registry, event::event_registry& events)
        : _registry{&registry}, _events{&events}
    {
        _subscription_handle = _events->dispatcher<ecs::component_added_event<ecs::entity, active_camera_component>>().subscribe(
            [this](const ecs::component_added_event<ecs::entity, active_camera_component>& evt) {
                _active_camera_entity = evt.entity;

                auto to_remove = tempest::vector<ecs::entity>();
                _registry->each([&](const ecs::self_component& self, const active_camera_component&) {
                    if (self.entity != evt.entity)
                    {
                        to_remove.push_back(self.entity);
                    }
                });
                for (auto entity : to_remove)
                {
                    _registry->remove<active_camera_component>(entity);
                }
            });
    }

    camera_system::camera_system(ecs::registry& registry)
        : camera_system(registry, registry.event_registry())
    {
    }

    camera_system::~camera_system()
    {
        if (_events != nullptr && _subscription_handle != event::null_subscription<ecs::component_added_event<ecs::entity, active_camera_component>>)
        {
            static_cast<void>(_events->dispatcher<ecs::component_added_event<ecs::entity, active_camera_component>>().unsubscribe(_subscription_handle));
        }
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
            if (_registry->has<active_camera_component>(entity) && _registry->has<camera_component>(entity))
            {
                return entity;
            }
        }

        auto found_active = tempest::optional<ecs::entity>();
        _registry->each([&](const ecs::self_component& self, const active_camera_component&, const camera_component&) {
            if (!found_active.has_value())
            {
                found_active = self.entity;
            }
        });

        if (found_active.has_value())
        {
            _active_camera_entity = found_active;
            return found_active;
        }

        auto fallback = tempest::optional<ecs::entity>();
        _registry->each([&](const ecs::self_component& self, const camera_component&, const ecs::transform_component&) {
            if (!fallback.has_value())
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
        const auto eye_pos = math::vec4<float>(pos.x, pos.y, pos.z, 1.0f);

        return render_camera{
            .proj = proj,
            .inv_proj = inv_proj,
            .view = view,
            .inv_view = inv_view,
            .eye_position = eye_pos,
        };
    }

    auto camera_system::set_active_camera(ecs::entity camera_entity) -> void
    {
        if (_registry == nullptr)
        {
            return;
        }

        _active_camera_entity = camera_entity;

        auto to_remove = tempest::vector<ecs::entity>();
        _registry->each([&](const ecs::self_component& self, const active_camera_component&) {
            if (self.entity != camera_entity)
            {
                to_remove.push_back(self.entity);
            }
        });
        for (auto entity : to_remove)
        {
            _registry->remove<active_camera_component>(entity);
        }

        if (!_registry->has<active_camera_component>(camera_entity))
        {
            _registry->assign(camera_entity, active_camera_component{});
        }
    }
} // namespace tempest::render_system
