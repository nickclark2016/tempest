#include <tempest/render_system/shadow_atlas_math.hpp>

#include <bit>
#include <cmath>
#include <tempest/algorithm.hpp>

namespace tempest::render_system
{
    auto compute_max_cascades_for_resolution(uint32_t cascade_res, uint32_t max_atlas_dim, uint32_t padding) noexcept
        -> uint32_t
    {
        if (cascade_res == 0 || max_atlas_dim == 0)
        {
            return 0;
        }

        const auto total_pad = padding * 2;
        if (max_atlas_dim <= total_pad || cascade_res > max_atlas_dim - total_pad)
        {
            return 0;
        }

        const auto padded_tile = cascade_res + total_pad;
        const auto tiles_per_axis = max_atlas_dim / padded_tile;
        return tiles_per_axis * tiles_per_axis;
    }

    auto compute_max_cascade_resolution(uint32_t num_cascades, uint32_t max_atlas_dim, uint32_t padding) noexcept
        -> uint32_t
    {
        if (num_cascades == 0 || max_atlas_dim == 0)
        {
            return 0;
        }

        const auto cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(num_cascades))));
        const auto rows = static_cast<uint32_t>(std::ceil(static_cast<float>(num_cascades) / static_cast<float>(cols)));

        const auto max_tile_w = max_atlas_dim / cols;
        const auto max_tile_h = max_atlas_dim / rows;
        const auto max_tile = tempest::min(max_tile_w, max_tile_h);

        const auto total_pad = padding * 2;
        if (max_tile <= total_pad)
        {
            return 0;
        }

        const auto available_res = max_tile - total_pad;
        if (available_res == 0)
        {
            return 0;
        }

        return std::bit_floor(available_res);
    }

    auto calculate_directional_shadow_atlas_plan(uint32_t cascade_res, uint32_t num_cascades, uint32_t max_atlas_dim,
                                                 uint32_t padding) noexcept -> shadow_atlas_plan
    {
        if (num_cascades == 0 || cascade_res == 0)
        {
            const auto fallback_size = max_atlas_dim > 0 ? tempest::min(max_atlas_dim, 512U) : 512U;
            return shadow_atlas_plan{
                .atlas_size = {fallback_size, fallback_size},
                .effective_cascade_resolution = 0,
                .was_clamped = false,
            };
        }

        const auto effective_max_dim = max_atlas_dim > 0 ? max_atlas_dim : 8192U;
        const auto cols = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(num_cascades))));
        const auto rows = static_cast<uint32_t>(std::ceil(static_cast<float>(num_cascades) / static_cast<float>(cols)));

        const auto total_pad = padding * 2;
        auto effective_res = cascade_res;
        auto was_clamped = false;

        const auto padded_tile = effective_res + total_pad;
        const auto req_w = cols * padded_tile;
        const auto req_h = rows * padded_tile;

        if (req_w > effective_max_dim || req_h > effective_max_dim)
        {
            const auto max_supported_res = compute_max_cascade_resolution(num_cascades, effective_max_dim, padding);
            if (max_supported_res > 0)
            {
                effective_res = tempest::min(cascade_res, max_supported_res);
                was_clamped = (effective_res != cascade_res);
            }
        }

        const auto final_padded_tile = effective_res + total_pad;
        const auto final_req_w = cols * final_padded_tile;
        const auto final_req_h = rows * final_padded_tile;

        auto atlas_w = std::bit_ceil(tempest::max(512U, final_req_w));
        auto atlas_h = std::bit_ceil(tempest::max(512U, final_req_h));

        atlas_w = tempest::min(atlas_w, effective_max_dim);
        atlas_h = tempest::min(atlas_h, effective_max_dim);

        return shadow_atlas_plan{
            .atlas_size = {atlas_w, atlas_h},
            .effective_cascade_resolution = effective_res,
            .was_clamped = was_clamped,
        };
    }
} // namespace tempest::render_system
