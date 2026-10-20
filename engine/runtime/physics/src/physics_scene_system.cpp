#include <tempest/physics/physics_scene_system.hpp>
#include <tempest/transformations.hpp>

namespace tempest::physics
{
    namespace
    {
        [[nodiscard]] inline auto to_shim_vec3(const math::vec3<float>& v) noexcept -> jolt::shim::vec3
        {
            return jolt::shim::vec3{.x = v.x, .y = v.y, .z = v.z};
        }

        [[nodiscard]] inline auto from_shim_vec3(const jolt::shim::vec3& v) noexcept -> math::vec3<float>
        {
            return math::vec3<float>{v.x, v.y, v.z};
        }

        [[nodiscard]] inline auto to_shim_quat(const math::quat<float>& q) noexcept -> jolt::shim::quat
        {
            return jolt::shim::quat{.x = q.x, .y = q.y, .z = q.z, .w = q.w};
        }

        [[nodiscard]] inline auto from_shim_quat(const jolt::shim::quat& q) noexcept -> math::quat<float>
        {
            return math::quat<float>{q.x, q.y, q.z, q.w};
        }

        struct sync_scope_guard
        {
            non_null<bool> flag;

            explicit sync_scope_guard(bool& target) noexcept : flag{target}
            {
                *flag = true;
            }

            sync_scope_guard(const sync_scope_guard&) = delete;
            sync_scope_guard(sync_scope_guard&&) noexcept = delete;
            auto operator=(const sync_scope_guard&) -> sync_scope_guard& = delete;
            auto operator=(sync_scope_guard&&) noexcept -> sync_scope_guard& = delete;

            ~sync_scope_guard() noexcept
            {
                *flag = false;
            }
        };
    } // namespace

    physics_scene_system::physics_scene_system(physics_world& world, ecs::archetype_registry& registry)
        : physics_scene_system(world, registry, registry.event_registry())
    {
    }

    physics_scene_system::physics_scene_system(physics_world& world, ecs::archetype_registry& registry,
                                               event::event_registry& event_registry)
        : _world{world},
          _registry{registry},
          _event_registry{event_registry}
    {
        _subscribe_events();
        init_from_registry();
    }

    physics_scene_system::~physics_scene_system()
    {
        _unsubscribe_events();
        clear();
    }

    auto physics_scene_system::_subscribe_events() -> void
    {
        _sub_box_added = _event_registry.get()
                             ->dispatcher<ecs::component_added_event<ecs::entity, box_collider_component>>()
                             .subscribe([this](const auto& evt) { _on_box_collider_added(evt); });
        _sub_box_removed =
            _event_registry.get()
                ->dispatcher<ecs::component_removed_event<ecs::entity, box_collider_component>>()
                .subscribe([this](const auto& evt) { _on_box_collider_removed(evt); });
        _sub_box_replaced =
            _event_registry.get()
                ->dispatcher<ecs::component_replaced_event<ecs::entity, box_collider_component>>()
                .subscribe([this](const auto& evt) { _on_box_collider_replaced(evt); });

        _sub_rb_added = _event_registry.get()
                            ->dispatcher<ecs::component_added_event<ecs::entity, rigid_body_component>>()
                            .subscribe([this](const auto& evt) { _on_rigid_body_added(evt); });
        _sub_rb_removed = _event_registry.get()
                              ->dispatcher<ecs::component_removed_event<ecs::entity, rigid_body_component>>()
                              .subscribe([this](const auto& evt) { _on_rigid_body_removed(evt); });
        _sub_rb_replaced =
            _event_registry.get()
                ->dispatcher<ecs::component_replaced_event<ecs::entity, rigid_body_component>>()
                .subscribe([this](const auto& evt) { _on_rigid_body_replaced(evt); });

        _sub_trans_replaced =
            _event_registry.get()
                ->dispatcher<ecs::component_replaced_event<ecs::entity, ecs::transform_component>>()
                .subscribe([this](const auto& evt) { _on_transform_replaced(evt); });

        _sub_entity_destroyed = _event_registry.get()
                                    ->dispatcher<ecs::entity_destroyed_event<ecs::entity>>()
                                    .subscribe([this](const auto& evt) { _on_entity_destroyed(evt); });
    }

    auto physics_scene_system::_unsubscribe_events() -> void
    {
        [[maybe_unused]] const auto unsub_box_added =
            _event_registry.get()
                ->dispatcher<ecs::component_added_event<ecs::entity, box_collider_component>>()
                .unsubscribe(_sub_box_added);
        [[maybe_unused]] const auto unsub_box_removed =
            _event_registry.get()
                ->dispatcher<ecs::component_removed_event<ecs::entity, box_collider_component>>()
                .unsubscribe(_sub_box_removed);
        [[maybe_unused]] const auto unsub_box_replaced =
            _event_registry.get()
                ->dispatcher<ecs::component_replaced_event<ecs::entity, box_collider_component>>()
                .unsubscribe(_sub_box_replaced);

        [[maybe_unused]] const auto unsub_rb_added =
            _event_registry.get()
                ->dispatcher<ecs::component_added_event<ecs::entity, rigid_body_component>>()
                .unsubscribe(_sub_rb_added);
        [[maybe_unused]] const auto unsub_rb_removed =
            _event_registry.get()
                ->dispatcher<ecs::component_removed_event<ecs::entity, rigid_body_component>>()
                .unsubscribe(_sub_rb_removed);
        [[maybe_unused]] const auto unsub_rb_replaced =
            _event_registry.get()
                ->dispatcher<ecs::component_replaced_event<ecs::entity, rigid_body_component>>()
                .unsubscribe(_sub_rb_replaced);

        [[maybe_unused]] const auto unsub_trans_replaced =
            _event_registry.get()
                ->dispatcher<ecs::component_replaced_event<ecs::entity, ecs::transform_component>>()
                .unsubscribe(_sub_trans_replaced);

        [[maybe_unused]] const auto unsub_entity_destroyed =
            _event_registry.get()
                ->dispatcher<ecs::entity_destroyed_event<ecs::entity>>()
                .unsubscribe(_sub_entity_destroyed);
    }

    auto physics_scene_system::pre_simulation_flush() -> void
    {
        auto* const system = _world->physics_system();
        if (system == nullptr)
        {
            return;
        }

        // Flush latest transform states for all kinematic bodies prior to simulation advance
        for (const auto& entry : _bodies)
        {
            const auto target_entity = entry.entity;
            const auto& body_data = entry.info;

            if (body_data.motion == jolt::shim::motion_type::kinematic)
            {
                if (const auto* transform = _registry->try_get<ecs::transform_component>(target_entity))
                {
                    auto offset = box_collider_component::default_offset;
                    if (const auto* collider = _registry->try_get<box_collider_component>(target_entity))
                    {
                        offset = collider->offset;
                    }

                    const auto world_pos = transform->position() + math::rotate(transform->rotation_quat(), offset);
                    const auto world_rot = transform->rotation_quat();

                    system->set_body_position(body_data.body, to_shim_vec3(world_pos));
                    system->set_body_rotation(body_data.body, to_shim_quat(world_rot));
                }
            }
        }
    }

    auto physics_scene_system::post_simulation_sync() -> void
    {
        auto* const system = _world->physics_system();
        if (system == nullptr)
        {
            return;
        }

        // Synchronize simulated dynamic poses back to transform components under internal sync guard
        const auto sync_guard = sync_scope_guard{_is_internal_physics_sync};

        for (const auto& entry : _bodies)
        {
            const auto target_entity = entry.entity;
            const auto& body_data = entry.info;

            if (body_data.motion == jolt::shim::motion_type::dynamic)
            {
                const auto jolt_pos = from_shim_vec3(system->get_body_position(body_data.body));
                const auto jolt_rot = from_shim_quat(system->get_body_rotation(body_data.body));

                auto offset = box_collider_component::default_offset;
                if (const auto* collider = _registry->try_get<box_collider_component>(target_entity))
                {
                    offset = collider->offset;
                }

                const auto entity_pos = jolt_pos - math::rotate(jolt_rot, offset);
                const auto entity_rot = jolt_rot;

                if (auto* transform = _registry->try_get<ecs::transform_component>(target_entity))
                {
                    transform->set(entity_pos, entity_rot);
                }

                if (auto* history = _registry->try_get<ecs::transform_history_component>(target_entity))
                {
                    history->current_position = entity_pos;
                    history->current_rotation = entity_rot;
                }
            }
        }
    }

    auto physics_scene_system::step(chrono::duration<double> delta_time) -> void
    {
        pre_simulation_flush();
        _world->step(delta_time);
        post_simulation_sync();
    }

    auto physics_scene_system::step(float delta_time) -> void
    {
        step(chrono::duration<double>{static_cast<double>(delta_time)});
    }

    auto physics_scene_system::init_from_registry() -> void
    {
        _registry->each([this](const ecs::self_component& self, const box_collider_component&) {
            _spawn_body(self.entity);
        });
    }

    auto physics_scene_system::clear() -> void
    {
        auto* const system = _world->physics_system();

        for (const auto& entry : _bodies)
        {
            const auto target_entity = entry.entity;
            const auto& body_data = entry.info;

            if (_registry->is_valid(target_entity))
            {
                if (auto* rb = _registry->try_get<rigid_body_component>(target_entity))
                {
                    rb->id = jolt::shim::invalid_body_id;
                }
            }

            if (system != nullptr)
            {
                system->remove_body(body_data.body);
                system->destroy_body(body_data.body);
                if (body_data.shape != nullptr)
                {
                    system->destroy_shape(body_data.shape);
                }
            }
        }

        _bodies.clear();
    }

    auto physics_scene_system::get_body_id(ecs::entity entity) const noexcept -> optional<jolt::shim::body_id>
    {
        for (const auto& entry : _bodies)
        {
            if (entry.entity == entity)
            {
                return entry.info.body;
            }
        }
        return nullopt;
    }

    auto physics_scene_system::get_entity(jolt::shim::body_id body) const noexcept -> optional<ecs::entity>
    {
        for (const auto& entry : _bodies)
        {
            if (entry.info.body == body)
            {
                return entry.entity;
            }
        }
        return nullopt;
    }

    auto physics_scene_system::get_body_info(ecs::entity entity) const noexcept -> optional<body_info>
    {
        for (const auto& entry : _bodies)
        {
            if (entry.entity == entity)
            {
                return entry.info;
            }
        }
        return nullopt;
    }

    auto physics_scene_system::has_body(ecs::entity entity) const noexcept -> bool
    {
        for (const auto& entry : _bodies)
        {
            if (entry.entity == entity)
            {
                return true;
            }
        }
        return false;
    }

    auto physics_scene_system::body_count() const noexcept -> size_t
    {
        return _bodies.size();
    }

    auto physics_scene_system::_spawn_body(ecs::entity entity) -> void
    {
        if (has_body(entity))
        {
            _despawn_body(entity);
        }

        const auto* collider = _registry->try_get<box_collider_component>(entity);
        if (collider == nullptr)
        {
            return;
        }

        auto* const system = _world->physics_system();
        if (system == nullptr)
        {
            return;
        }

        const auto shape = system->create_box_shape(to_shim_vec3(collider->half_extents));

        auto motion = rigid_body_component::default_motion;
        auto layer = rigid_body_component::default_layer;
        auto mass = rigid_body_component::default_mass;

        if (const auto* rb = _registry->try_get<rigid_body_component>(entity))
        {
            motion = rb->motion;
            layer = rb->layer;
            mass = rb->mass;
        }

        auto base_pos = math::vec3<float>{0.0F, 0.0F, 0.0F};
        auto base_rot = math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F};

        if (const auto* transform = _registry->try_get<ecs::transform_component>(entity))
        {
            base_pos = transform->position();
            base_rot = transform->rotation_quat();
        }

        const auto world_pos = base_pos + math::rotate(base_rot, collider->offset);
        const auto world_rot = base_rot;

        const auto description = jolt::shim::body_desc{
            .shape = shape,
            .position = to_shim_vec3(world_pos),
            .rotation = to_shim_quat(world_rot),
            .motion = motion,
            .layer = layer,
            .mass = mass,
        };

        const auto created_body = system->create_body(description);
        const auto activate = (motion != jolt::shim::motion_type::static_motion);
        system->add_body(created_body, activate);

        if (auto* rb = _registry->try_get<rigid_body_component>(entity))
        {
            rb->id = created_body;
        }

        _bodies.push_back(body_entry{
            .entity = entity,
            .info = body_info{
                .body = created_body,
                .shape = shape,
                .motion = motion,
            },
        });
    }

    auto physics_scene_system::_despawn_body(ecs::entity entity) -> void
    {
        auto it = _bodies.begin();
        while (it != _bodies.end())
        {
            if (it->entity == entity)
            {
                break;
            }
            ++it;
        }

        if (it == _bodies.end())
        {
            return;
        }

        const auto info = it->info;
        _bodies.erase(it);

        if (_registry->is_valid(entity))
        {
            if (auto* rb = _registry->try_get<rigid_body_component>(entity))
            {
                if (rb->id == info.body)
                {
                    rb->id = jolt::shim::invalid_body_id;
                }
            }
        }

        auto* const system = _world->physics_system();
        if (system != nullptr)
        {
            system->remove_body(info.body);
            system->destroy_body(info.body);
            if (info.shape != nullptr)
            {
                system->destroy_shape(info.shape);
            }
        }
    }

    auto physics_scene_system::_on_box_collider_added(
        const ecs::component_added_event<ecs::entity, box_collider_component>& evt) -> void
    {
        _spawn_body(evt.entity);
    }

    auto physics_scene_system::_on_box_collider_removed(
        const ecs::component_removed_event<ecs::entity, box_collider_component>& evt) -> void
    {
        _despawn_body(evt.entity);
    }

    auto physics_scene_system::_on_box_collider_replaced(
        const ecs::component_replaced_event<ecs::entity, box_collider_component>& evt) -> void
    {
        _despawn_body(evt.entity);
        _spawn_body(evt.entity);
    }

    auto physics_scene_system::_on_rigid_body_added(
        const ecs::component_added_event<ecs::entity, rigid_body_component>& evt) -> void
    {
        if (_registry->has<box_collider_component>(evt.entity))
        {
            _despawn_body(evt.entity);
            _spawn_body(evt.entity);
        }
    }

    auto physics_scene_system::_on_rigid_body_removed(
        const ecs::component_removed_event<ecs::entity, rigid_body_component>& evt) -> void
    {
        _despawn_body(evt.entity);
        if (_registry->has<box_collider_component>(evt.entity))
        {
            _spawn_body(evt.entity);
        }
    }

    auto physics_scene_system::_on_rigid_body_replaced(
        const ecs::component_replaced_event<ecs::entity, rigid_body_component>& evt) -> void
    {
        if (_registry->has<box_collider_component>(evt.entity))
        {
            _despawn_body(evt.entity);
            _spawn_body(evt.entity);
        }
    }

    auto physics_scene_system::_on_transform_replaced(
        const ecs::component_replaced_event<ecs::entity, ecs::transform_component>& evt) -> void
    {
        if (_is_internal_physics_sync)
        {
            return;
        }

        auto* const system = _world->physics_system();
        if (system == nullptr)
        {
            return;
        }

        const auto body_id_opt = get_body_id(evt.entity);
        if (!body_id_opt.has_value())
        {
            return;
        }

        auto offset = box_collider_component::default_offset;
        if (const auto* collider = _registry->try_get<box_collider_component>(evt.entity))
        {
            offset = collider->offset;
        }

        const auto world_pos = evt.new_component.position() + math::rotate(evt.new_component.rotation_quat(), offset);
        const auto world_rot = evt.new_component.rotation_quat();

        system->set_body_position(*body_id_opt, to_shim_vec3(world_pos));
        system->set_body_rotation(*body_id_opt, to_shim_quat(world_rot));
    }

    auto physics_scene_system::_on_entity_destroyed(const ecs::entity_destroyed_event<ecs::entity>& evt) -> void
    {
        _despawn_body(evt.entity);
    }
} // namespace tempest::physics
