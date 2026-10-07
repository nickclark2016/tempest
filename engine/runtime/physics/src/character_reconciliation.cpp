#include <tempest/physics/character_reconciliation.hpp>

#include <tempest/algorithm.hpp>
#include <tempest/math_utils.hpp>
#include <tempest/transform_component.hpp>

#include <tempest/transform_history_component.hpp>

namespace tempest::physics
{
    auto reconcile_client_character(physics_world& world,
                                    ecs::archetype_registry& registry,
                                    ecs::entity player_entity,
                                    character_prediction_buffer& buffer,
                                    const character_snapshot& server_snapshot,
                                    const reconciliation_config& config) -> reconciliation_result
    {
        auto result = reconciliation_result{};
        const auto server_tick = server_snapshot.tick();

        const auto* const predicted_frame = buffer.get_frame(server_tick);
        if (predicted_frame == nullptr)
        {
            buffer.discard_acked_inputs(server_tick);
            return result;
        }

        const auto pos_delta = server_snapshot.position() - predicted_frame->predicted_state.position();
        const auto error_dist = math::length(pos_delta);
        result.error_distance = error_dist;

        if (error_dist > config.error_threshold)
        {
            result.diverged = true;
            result.reconciled = true;

            const auto is_snap = (error_dist > config.snap_threshold);
            result.snapped = is_snap;

            // 1. Update visual smoothing error if smoothing component is attached
            if (const auto* const smoothing = registry.try_get<reconciliation_smoothing_component>(player_entity))
            {
                auto updated_smoothing = *smoothing;
                if (is_snap)
                {
                    updated_smoothing.position_error = math::vec3<float>{0.0F, 0.0F, 0.0F};
                    updated_smoothing.rotation_error = math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F};
                }
                else
                {
                    updated_smoothing.position_error = updated_smoothing.position_error + pos_delta;

                    const auto q_pred_inv = math::inverse(predicted_frame->predicted_state.rotation());
                    const auto q_diff = server_snapshot.rotation() * q_pred_inv;
                    updated_smoothing.rotation_error = math::normalize(q_diff * updated_smoothing.rotation_error);
                }
                registry.replace(player_entity, updated_smoothing);
            }

            // 2. Rollback physical state to authoritative server snapshot
            restore_character_snapshot(registry, player_entity, server_snapshot, &world);

            // 3. Resimulate forward from server_tick + 1 up to latest_tick
            if (const auto latest = buffer.latest_tick())
            {
                const auto latest_tick = *latest;
                for (auto tick = server_tick + 1U; tick <= latest_tick; ++tick)
                {
                    if (auto* const frame = buffer.get_frame(tick))
                    {
                        step_character_simulation(world, registry, player_entity, frame->cmd,
                                                  config.fixed_delta_time, config.gravity);
                        frame->predicted_state = capture_character_snapshot(tick, registry, player_entity);
                        ++result.resimulated_ticks;
                    }
                }
            }
        }

        // 4. Discard acknowledged frames up to server tick
        buffer.discard_acked_inputs(server_tick);

        return result;
    }

    auto update_reconciliation_smoothing(ecs::archetype_registry& registry,
                                         chrono::duration<double> delta_time) -> void
    {
        const auto dt = static_cast<float>(delta_time.count());
        if (dt <= 0.0F)
        {
            return;
        }

        registry.each([dt](reconciliation_smoothing_component& smoothing,
                           const ecs::transform_history_component& history,
                           ecs::transform_component& transform) {
            const auto decay = 1.0F - math::exp(-smoothing.decay_rate * dt);

            smoothing.position_error = math::lerp(smoothing.position_error, math::vec3<float>{0.0F, 0.0F, 0.0F}, decay);
            smoothing.rotation_error = math::slerp(smoothing.rotation_error, math::quat<float>{0.0F, 0.0F, 0.0F, 1.0F}, decay);

            transform.set(history.render_position + smoothing.position_error,
                          smoothing.rotation_error * history.render_rotation);
        });
    }
} // namespace tempest::physics
