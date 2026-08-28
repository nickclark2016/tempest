#ifndef tempest_render_system_shadow_atlas_math_hpp
#define tempest_render_system_shadow_atlas_math_hpp

#include <tempest/api.hpp>
#include <tempest/vec2.hpp>

#include <cstdint>

namespace tempest::render_system
{
    struct TEMPEST_API shadow_atlas_plan
    {
        math::vec2<uint32_t> atlas_size{0, 0};
        uint32_t effective_cascade_resolution{0};
        bool was_clamped{false};
    };

    /// @brief Computes the maximum number of shadow cascades of given resolution that can fit inside an atlas with max
    /// dimension.
    /// @param cascade_res The resolution of a single square cascade.
    /// @param max_atlas_dim The maximum supported texture dimension (e.g. from device limits).
    /// @param padding Border padding around each cascade tile in pixels.
    /// @return The maximum number of cascades that can be packed.
    [[nodiscard]] TEMPEST_API auto compute_max_cascades_for_resolution(uint32_t cascade_res, uint32_t max_atlas_dim,
                                                                       uint32_t padding = 4) noexcept -> uint32_t;

    /// @brief Computes the maximum power-of-two cascade resolution for a given number of cascades to fit in the atlas.
    /// @param num_cascades The desired number of cascades.
    /// @param max_atlas_dim The maximum supported texture dimension.
    /// @param padding Border padding around each cascade tile in pixels.
    /// @return The largest power-of-two resolution (e.g. 512, 1024, 2048, 4096) such that num_cascades fit.
    [[nodiscard]] TEMPEST_API auto compute_max_cascade_resolution(uint32_t num_cascades, uint32_t max_atlas_dim,
                                                                  uint32_t padding = 4) noexcept -> uint32_t;

    /// @brief Computes the optimal power-of-two shadow atlas dimensions and effective cascade resolution.
    /// @param cascade_res The requested per-cascade resolution.
    /// @param num_cascades The requested number of cascades.
    /// @param max_atlas_dim The maximum supported texture dimension.
    /// @param padding Border padding around each cascade tile in pixels.
    /// @return A shadow_atlas_plan containing the power-of-two atlas dimensions, effective resolution, and clamping
    /// status.
    [[nodiscard]] TEMPEST_API auto calculate_directional_shadow_atlas_plan(uint32_t cascade_res, uint32_t num_cascades,
                                                                           uint32_t max_atlas_dim,
                                                                           uint32_t padding = 4) noexcept
        -> shadow_atlas_plan;
} // namespace tempest::render_system

#endif // tempest_render_system_shadow_atlas_math_hpp
