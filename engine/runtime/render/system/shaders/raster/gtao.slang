import "../common/consts";
import "../common/fsquad";
import "../common/material";
import "../common/scene";

struct GTAOPushConstants {
    uint64_t scene_constants_address;
    uint32_t depth_texture_index;
    uint32_t point_sampler_index;

    uint32_t slice_count;
    uint32_t steps_per_slice;

    float radius;
    float thickness;
    float power;
    
    uint32_t _pad0;
    uint32_t _pad1;
    uint32_t _pad2;
};

[[vk::push_constant]]
GTAOPushConstants gtao_push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(uint id : SV_VertexID) {
    let vtx = generate_quad_vertex(id);
    return VSOutput(vtx.position, vtx.uv);
}

float retrieve_frag_depth(float2 uv, Texture2D depth_texture, SamplerState depth_sampler) {
    return depth_texture.SampleLevel(depth_sampler, uv, 0).r;
}

struct UnprojectParams {
    float inv_p00;
    float inv_p11;
    float inv_near;
};

float3 reconstruct_view_position(float2 uv, float frag_depth, UnprojectParams params) {
    let neg_zv = 1.0f / max(frag_depth * params.inv_near, 1e-6f);
    let xy = uv * 2.0f - 1.0f;
    return float3(xy.x * neg_zv * params.inv_p00, xy.y * neg_zv * params.inv_p11, -neg_zv);
}

// TODO: Use normal from G-buffer if available for more accurate reconstruction
// This should allow for GTAO to respect shading normals instead of just geometric normals
float3 reconstruct_view_normal(float2 uv, float center_depth, UnprojectParams params, Texture2D depth_tex, SamplerState point_sampler, float2 inv_screen_size) {
    let uv_l = uv - float2(inv_screen_size.x, 0.0f);
    let uv_r = uv + float2(inv_screen_size.x, 0.0f);
    let uv_u = uv - float2(0.0f, inv_screen_size.y);
    let uv_d = uv + float2(0.0f, inv_screen_size.y);
    let z_l = retrieve_frag_depth(uv_l, depth_tex, point_sampler);
    let z_r = retrieve_frag_depth(uv_r, depth_tex, point_sampler);
    let z_u = retrieve_frag_depth(uv_u, depth_tex, point_sampler);
    let z_d = retrieve_frag_depth(uv_d, depth_tex, point_sampler);
    let p_c = reconstruct_view_position(uv, center_depth, params);
    let p_l = reconstruct_view_position(uv_l, z_l, params);
    let p_r = reconstruct_view_position(uv_r, z_r, params);
    let p_u = reconstruct_view_position(uv_u, z_u, params);
    let p_d = reconstruct_view_position(uv_d, z_d, params);
    // Select neighbor with smallest depth step to avoid crossing silhouettes
    let diff_r = select(z_r > 1e-5f, abs(z_r - center_depth), 1e20f);
    let diff_l = select(z_l > 1e-5f, abs(z_l - center_depth), 1e20f);
    let diff_d = select(z_d > 1e-5f, abs(z_d - center_depth), 1e20f);
    let diff_u = select(z_u > 1e-5f, abs(z_u - center_depth), 1e20f);
    let dp_dx = (diff_r < diff_l) ? (p_r - p_c) : (p_c - p_l);
    let dp_dy = (diff_d < diff_u) ? (p_d - p_c) : (p_c - p_u);
    let len_dx_sq = dot(dp_dx, dp_dx);
    let len_dy_sq = dot(dp_dy, dp_dy);
    if (len_dx_sq < 1e-12f || len_dy_sq < 1e-12f) {
        return float3(0.0f, 0.0f, 1.0f);
    }
    let dir_x = dp_dx * rsqrt(len_dx_sq);
    let dir_y = dp_dy * rsqrt(len_dy_sq);
    let normal = cross(dir_y, dir_x);
    let len_sq = dot(normal, normal);
    return select(len_sq < 1e-8f, float3(0.0f, 0.0f, 1.0f), normal * rsqrt(max(len_sq, 1e-8f)));
}

float interleaved_gradient_noise(float2 screen_position) {
    let magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    return frac(magic.z * frac(dot(screen_position, magic.xy)));
}

uint32_t update_sectors<let N : uint32_t>(float min_horizon, float max_horizon, uint32_t current_bitfield) {
    // Validate horizons
    if (min_horizon >= max_horizon || min_horizon >= 1.0f || max_horizon <= 0.0f) {
        return current_bitfield;
    }

    // Clamp to prevent out-of-bounds bit indices
    let start_bit = min(uint(max(floor(min_horizon * float(N)), 0.0f)), N - 1);
    let end_bit = min(uint(max(ceil(max_horizon * float(N)), 0.0f)), N);

    if (end_bit <= start_bit) {
        return current_bitfield;
    }

    let span = end_bit - start_bit;
    let span_mask = select(span >= 32, 0xFFFFFFFFu, (1u << min(span, 31u)) - 1u);
    let sample_bitmask = span_mask << start_bit;
    return current_bitfield | sample_bitmask;
}

void march_slice_direction(
        float3 view_position,
        float3 view_normal,
        float3 view_direction,
        float3 n_proj,
        float3 n_tangent,
        float2 target_size,
        float2 inv_target_size,
        float2 uv,
        float2 uv_direction,
        float sampling_direction,
        float step_offset,
        uint32_t step_count,
        float thickness,
        float radius,
        UnprojectParams unproject_params,
        Texture2D depth_texture,
        SamplerState depth_sampler,
        inout uint visibility_mask) {
    let min_step_scale = length(inv_target_size) / max(length(uv_direction), 1e-6f);
    if (min_step_scale > 1.0f) {
        return;
    }

    let inv_step_count = 1.0f / (float(step_count) + 0.1f);
    for (var step = 0u; step < step_count; ++step) {
        let t = (float(step) + step_offset + 0.1f) * inv_step_count;
        let step_scale = lerp(min_step_scale, 1.0f, t * t);
        let sample_uv = uv + uv_direction * (step_scale * sampling_direction);
        
        if (any(sample_uv < 0.0f || sample_uv > 1.0f)) {
            break; // This will never go back into the valid UV range
        }

        let snapped_uv = (floor(sample_uv * target_size) + 0.5f) * inv_target_size;
        let sample_depth = retrieve_frag_depth(snapped_uv, depth_texture, depth_sampler);
        
        // We're hitting the skybox or extremely far away geometry
        if (sample_depth <= 1e-5f) {
            continue;
        }
        let sample_view_position = reconstruct_view_position(snapped_uv, sample_depth, unproject_params);
        let delta_front = sample_view_position - view_position;
        let dist_sq = dot(delta_front, delta_front);

        // Check to see if the sample point is within the valid range and above the tangent plane
        if (dist_sq <= 1e-6f || dist_sq > radius * radius || dot(delta_front, view_normal) <= 1e-4f) {
            continue;
        }

        let dist_ratio_sq = dist_sq / (radius * radius);
        let effective_thickness = thickness * (1.0f - 0.5f * dist_ratio_sq);

        // Compute the position of the sample point relative to the back of the slice
        let delta_back = delta_front - view_direction * effective_thickness;

        let psi_front = clamp(atan2(dot(delta_front, n_tangent), dot(delta_front, n_proj)), -PI * 0.5f, PI * 0.5f);
        let psi_back = clamp(atan2(dot(delta_back, n_tangent), dot(delta_back, n_proj)), -PI * 0.5f, PI * 0.5f);
        let horizons = sin(float2(psi_front, psi_back)) * 0.5f + 0.5f;

        let clamped_horizons = clamp(horizons, 0.0f, 1.0f);

        let min_h = min(clamped_horizons.x, clamped_horizons.y);
        let max_h = max(clamped_horizons.x, clamped_horizons.y);

        visibility_mask = update_sectors<32>(min_h, max_h, visibility_mask);
    }
}

struct FragmentOutput {
    float visibility : SV_Target0; // R8_unorm
};

[shader("fragment")]
FragmentOutput FSMain(VSOutput vs_out) {
    let scene = (SceneGlobals*)gtao_push_constants.scene_constants_address;
    let point_sampler = bindless_samplers[gtao_push_constants.point_sampler_index];
    let depth_texture = bindless_textures[gtao_push_constants.depth_texture_index];
    let depth_value = retrieve_frag_depth(vs_out.uv, depth_texture, point_sampler);

    // If the depth value is too small, it's either the clear value or the background, so we can early out.
    if (depth_value <= 1e-5f) {
        return FragmentOutput(1.0f);
    }

    let unproject_params = UnprojectParams(
        scene.camera.inv_projection[0][0],
        scene.camera.inv_projection[1][1],
        scene.camera.inv_projection[3][2]
    );

    let view_position = reconstruct_view_position(vs_out.uv, depth_value, unproject_params);
    let view_normal = reconstruct_view_normal(vs_out.uv, depth_value, unproject_params, depth_texture, point_sampler, scene.inv_screen_size);
    let view_direction = normalize(-view_position);

    let noise_angle = interleaved_gradient_noise(vs_out.position.xy);
    let base_angle = noise_angle * PI;

    // Spatial Hash Offset with prime spatial offset
    let noise_step = interleaved_gradient_noise(vs_out.position.xy + float2(47.0f, 17.0f));

    let proj_radius = (gtao_push_constants.radius / max(-view_position.z, 0.1f)) * 0.5f;
    let radius_uv = proj_radius * float2(scene.camera.projection[0][0], abs(scene.camera.projection[1][1]));
    let slice_count = max(gtao_push_constants.slice_count, 1u); // Ensure there is at least one slice to sample
    let step_count = max(gtao_push_constants.steps_per_slice, 1u); // Ensure there is at least one step to sample in each slice
    
    var total_visibility = 0.0f;
    var total_weight = 0.0f;

    let inv_slice_count = rcp(float(slice_count));
    let inv_sector_count = rcp(32.0f);

    let sample_origin = view_position + view_normal * (1e-4f * -view_position.z);
    for (var slice = 0u; slice < slice_count; ++slice) {
        let phi = base_angle + float(slice) * PI * inv_slice_count;
        let dir = float2(cos(phi), sin(phi));
        let dir_uv = float2(dir.x, -dir.y) * radius_uv; // Flip Y to account for screen space UV orientation

        let plane_normal = normalize(cross(float3(dir, 0.0f), view_direction));
        let slice_tangent = cross(view_direction, plane_normal);
        let proj_normal = view_normal - plane_normal * dot(view_normal, plane_normal);
        let proj_normal_len = length(proj_normal);
        if (proj_normal_len <= 1e-4f) {
            continue;
        }
        let n_proj = proj_normal / proj_normal_len;
        let normal_angle = atan2(dot(n_proj, slice_tangent), dot(n_proj, view_direction));
        let n_tangent = -sin(normal_angle) * view_direction + cos(normal_angle) * slice_tangent;

        var slice_bitfield = 0u;
        
        // March the slice in the positive direction along the slice normal
        march_slice_direction(sample_origin,
            view_normal,
            view_direction,
            n_proj,
            n_tangent,
            scene.screen_size,
            scene.inv_screen_size,
            vs_out.uv,
            dir_uv,
            1.0f,
            noise_step,
            step_count,
            gtao_push_constants.thickness,
            gtao_push_constants.radius,
            unproject_params,
            depth_texture,
            point_sampler,
            slice_bitfield);

        // March the slice in the negative direction along the slice normal
        march_slice_direction(sample_origin,
            view_normal,
            view_direction,
            n_proj,
            n_tangent,
            scene.screen_size,
            scene.inv_screen_size,
            vs_out.uv,
            dir_uv,
            -1.0f,
            frac(noise_step + 0.5f),
            step_count,
            gtao_push_constants.thickness,
            gtao_push_constants.radius,
            unproject_params,
            depth_texture,
            point_sampler,
            slice_bitfield);
        
        let occluded_sectors = countbits(slice_bitfield);
        let slice_visibility = 1.0f - float(occluded_sectors) * inv_sector_count;
        total_visibility += slice_visibility * proj_normal_len;
        total_weight += proj_normal_len;
    }

    let average_visibility = select(total_weight > 1e-5f, clamp(total_visibility / total_weight, 0.0f, 1.0f), 1.0f);
    let power = select(gtao_push_constants.power > 0.0f, gtao_push_constants.power, 1.5f);
    let final_visibility = pow(average_visibility, power);
    return FragmentOutput(final_visibility);
}