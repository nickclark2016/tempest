#ifndef TEMPEST_PHYSICS_PHYSICS_SCENE_SYSTEM_HPP
#define TEMPEST_PHYSICS_PHYSICS_SCENE_SYSTEM_HPP

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/checked.hpp>
#include <tempest/chrono.hpp>
#include <tempest/ecs_events.hpp>
#include <tempest/event_registry.hpp>
#include <tempest/int.hpp>
#include <tempest/optional.hpp>
#include <tempest/physics/box_collider_component.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/physics/rigid_body_component.hpp>
#include <tempest/physics/shim/jolt_shim.hpp>
#include <tempest/transform_component.hpp>
#include <tempest/transform_history_component.hpp>
#include <tempest/vector.hpp>

namespace tempest::physics
{
    struct physics_body_info
    {
        jolt::shim::body_id body = jolt::shim::invalid_body_id;
        jolt::shim::shape_handle shape = nullptr;
        jolt::shim::motion_type motion = jolt::shim::motion_type::static_motion;
    };

    class TEMPEST_API physics_scene_system
    {
      public:
        using body_info = physics_body_info;

        static constexpr bool default_is_internal_physics_sync = false;

        physics_scene_system(physics_world& world, ecs::archetype_registry& registry);
        physics_scene_system(physics_world& world, ecs::archetype_registry& registry,
                             event::event_registry& event_registry);

        physics_scene_system(const physics_scene_system&) = delete;
        physics_scene_system(physics_scene_system&&) noexcept = delete;
        auto operator=(const physics_scene_system&) -> physics_scene_system& = delete;
        auto operator=(physics_scene_system&&) noexcept -> physics_scene_system& = delete;
        ~physics_scene_system();

        auto step(chrono::duration<double> delta_time) -> void;
        auto step(float delta_time) -> void;

        auto pre_simulation_flush() -> void;
        auto post_simulation_sync() -> void;

        auto init_from_registry() -> void;
        auto clear() -> void;

        [[nodiscard]] auto get_body_id(ecs::entity entity) const noexcept -> optional<jolt::shim::body_id>;
        [[nodiscard]] auto get_entity(jolt::shim::body_id body) const noexcept -> optional<ecs::entity>;
        [[nodiscard]] auto get_body_info(ecs::entity entity) const noexcept -> optional<body_info>;
        [[nodiscard]] auto has_body(ecs::entity entity) const noexcept -> bool;
        [[nodiscard]] auto body_count() const noexcept -> size_t;

      private:
        struct body_entry
        {
            ecs::entity entity = ecs::tombstone;
            body_info info{};
        };

        non_null<physics_world> _world;
        non_null<ecs::archetype_registry> _registry;
        non_null<event::event_registry> _event_registry;

        bool _is_internal_physics_sync = default_is_internal_physics_sync;

        vector<body_entry> _bodies;

        event::subscription_handle<ecs::component_added_event<ecs::entity, box_collider_component>> _sub_box_added;
        event::subscription_handle<ecs::component_removed_event<ecs::entity, box_collider_component>> _sub_box_removed;
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, box_collider_component>> _sub_box_replaced;
        event::subscription_handle<ecs::component_added_event<ecs::entity, rigid_body_component>> _sub_rb_added;
        event::subscription_handle<ecs::component_removed_event<ecs::entity, rigid_body_component>> _sub_rb_removed;
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, rigid_body_component>> _sub_rb_replaced;
        event::subscription_handle<ecs::component_replaced_event<ecs::entity, ecs::transform_component>> _sub_trans_replaced;
        event::subscription_handle<ecs::entity_destroyed_event<ecs::entity>> _sub_entity_destroyed;

        auto _subscribe_events() -> void;
        auto _unsubscribe_events() -> void;

        auto _spawn_body(ecs::entity entity) -> void;
        auto _despawn_body(ecs::entity entity) -> void;

        auto _on_box_collider_added(const ecs::component_added_event<ecs::entity, box_collider_component>& evt) -> void;
        auto _on_box_collider_removed(const ecs::component_removed_event<ecs::entity, box_collider_component>& evt) -> void;
        auto _on_box_collider_replaced(const ecs::component_replaced_event<ecs::entity, box_collider_component>& evt) -> void;
        auto _on_rigid_body_added(const ecs::component_added_event<ecs::entity, rigid_body_component>& evt) -> void;
        auto _on_rigid_body_removed(const ecs::component_removed_event<ecs::entity, rigid_body_component>& evt) -> void;
        auto _on_rigid_body_replaced(const ecs::component_replaced_event<ecs::entity, rigid_body_component>& evt) -> void;
        auto _on_transform_replaced(const ecs::component_replaced_event<ecs::entity, ecs::transform_component>& evt) -> void;
        auto _on_entity_destroyed(const ecs::entity_destroyed_event<ecs::entity>& evt) -> void;
    };
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_PHYSICS_SCENE_SYSTEM_HPP
