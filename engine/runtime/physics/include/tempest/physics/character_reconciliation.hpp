#ifndef TEMPEST_PHYSICS_CHARACTER_RECONCILIATION_HPP
#define TEMPEST_PHYSICS_CHARACTER_RECONCILIATION_HPP

#include <tempest/api.hpp>
#include <tempest/archetype.hpp>
#include <tempest/chrono.hpp>
#include <tempest/int.hpp>
#include <tempest/physics/character_prediction.hpp>
#include <tempest/physics/character_snapshot.hpp>
#include <tempest/physics/physics_world.hpp>
#include <tempest/quat.hpp>
#include <tempest/vec3.hpp>

namespace tempest::physics
{
    /// @brief Attached to predicted client entities to absorb state divergence offsets and smoothly
    /// decay them across render frames.
    struct alignas(16) reconciliation_smoothing_component
    {
        static constexpr float default_decay_rate = 20.0F;

        math::vec3<float> position_error{0.0F, 0.0F, 0.0F};
        math::quat<float> rotation_error{0.0F, 0.0F, 0.0F, 1.0F};
        float decay_rate = default_decay_rate;
    };

    /// @brief Configuration settings controlling reconciliation tolerances, snap thresholds, and simulation rate.
    struct reconciliation_config
    {
        static constexpr float default_error_threshold = 0.02F; // 2cm
        static constexpr float default_snap_threshold = 1.25F;   // 1.25m
        static constexpr double default_fixed_step = 1.0 / 60.0; // 60Hz

        float error_threshold = default_error_threshold;
        float snap_threshold = default_snap_threshold;
        chrono::duration<double> fixed_delta_time{default_fixed_step};
        math::vec3<float> gravity{0.0F, -9.81F, 0.0F};
    };

    /// @brief Metrics and diagnostics produced by a reconciliation check against an authoritative server snapshot.
    struct reconciliation_result
    {
        bool reconciled = false;
        bool diverged = false;
        bool snapped = false;
        uint32_t resimulated_ticks = 0;
        float error_distance = 0.0F;
    };

    /// @brief Reconciles a client-predicted character entity against an incoming authoritative server snapshot.
    /// If the predicted state at the snapshot's tick diverges from the server snapshot by more than config.error_threshold,
    /// the character's physical state and transform history are restored to the server snapshot, unacknowledged inputs
    /// are resimulated forward up to buffer.latest_tick(), and prediction frames are updated.
    /// If divergence is <= config.snap_threshold, the position and rotation error is absorbed into
    /// reconciliation_smoothing_component for exponential visual decay. If divergence > config.snap_threshold,
    /// it snaps immediately without visual smoothing.
    /// All inputs acknowledged up to server_snapshot.tick() are discarded from the prediction buffer.
    /// @param world The physics world simulation instance.
    /// @param registry Archetype registry containing the player entity and components.
    /// @param player_entity Entity handle of the character being reconciled.
    /// @param buffer Circular client prediction buffer storing predicted inputs and frames.
    /// @param server_snapshot Authoritative server snapshot.
    /// @param config Reconciliation tolerances, fixed delta time, and gravity settings.
    /// @return Diagnostic reconciliation result metrics.
    TEMPEST_API auto reconcile_client_character(physics_world& world,
                                                ecs::archetype_registry& registry,
                                                ecs::entity player_entity,
                                                character_prediction_buffer& buffer,
                                                const character_snapshot& server_snapshot,
                                                const reconciliation_config& config = {}) -> reconciliation_result;

    /// @brief Decays visual error offsets on all entities with reconciliation_smoothing_component and applies
    /// them directly to ecs::transform_component. Does not affect physical simulation state or transform history.
    /// Typically invoked during render frame updates after transform interpolation.
    /// @param registry Archetype registry containing entities to smooth.
    /// @param delta_time Real render frame delta time in duration format.
    TEMPEST_API auto update_reconciliation_smoothing(ecs::archetype_registry& registry,
                                                     chrono::duration<double> delta_time) -> void;

    inline auto update_reconciliation_smoothing(ecs::archetype_registry& registry,
                                                 float delta_time) -> void
    {
        update_reconciliation_smoothing(registry, chrono::duration<double>{static_cast<double>(delta_time)});
    }
} // namespace tempest::physics

#endif // TEMPEST_PHYSICS_CHARACTER_RECONCILIATION_HPP
