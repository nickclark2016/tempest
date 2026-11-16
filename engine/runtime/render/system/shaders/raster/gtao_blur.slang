import "../common/fsquad";
import "../common/material";

struct GTAOBlurPushConstants {
    uint32_t depth_texture;
    uint32_t gtao_blur_input_texture;
    uint32_t point_sampler_index;
    float depth_discontinuity_threshold;
    float2 blur_direction;
};

[[vk::push_constant]]
GTAOBlurPushConstants gtao_blur_push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VSOutput VSMain(uint id : SV_VertexID) {
    let vtx = generate_quad_vertex(id);
    return VSOutput(vtx.position, vtx.uv);
}

struct FragmentOutput {
    float ao : SV_Target0;
};

static const float gaussian_kernel_weights[5] = {
    0.204163f, // offset 0
    0.180174f, // offset 1
    0.123832f, // offset 2
    0.066282f, // offset 3
    0.027631f  // offset 4
};

void accumulate_tap_w(float sample_ao, float sample_w, bool valid, float center_w, float dw, float neg_inv_sigma_log2e, float step_offset, float spatial_weight, inout float total_ao, inout float total_weight) {
    let expected_w = center_w + dw * step_offset;
    let delta_w = abs(sample_w - expected_w);

    let depth_weight = exp2(delta_w * neg_inv_sigma_log2e);
    let weight = select(valid, spatial_weight * depth_weight, 0.0f);

    total_ao += weight * sample_ao;
    total_weight += weight;

}

void accumulate_tap(float sample_ao, float sample_depth, bool in_bounds, float center_w, float dw, float neg_inv_sigma_log2e, float step_offset, float spatial_weight, inout float total_ao, inout float total_weight) {
    let valid = in_bounds && (sample_depth > 1e-5f);
    let safe_depth = select(valid, sample_depth, 1.0f);
    let sample_w = 1.0f / max(safe_depth, 1e-6f);

    accumulate_tap_w(sample_ao, sample_w, valid, center_w, dw, neg_inv_sigma_log2e, step_offset, spatial_weight, total_ao, total_weight);
}

[shader("fragment")]
FragmentOutput FSMain(VSOutput input) {
    let point_sampler = bindless_samplers[gtao_blur_push_constants.point_sampler_index];
    let depth_texture = bindless_textures[gtao_blur_push_constants.depth_texture];
    
    let center_depth = depth_texture.SampleLevel(point_sampler, input.uv, 0).r;

    // Discard fragments with invalid depth to avoid blurring artifacts.
    if (center_depth <= 1e-5f) {
        return FragmentOutput(1.0f);
    }

    let input_ao_texture = bindless_textures[gtao_blur_push_constants.gtao_blur_input_texture];
    let blur_dir = gtao_blur_push_constants.blur_direction;

    let center_ao = input_ao_texture.SampleLevel(point_sampler, input.uv, 0).r;
    let uv_p1 = input.uv + blur_dir;
    let uv_m1 = input.uv - blur_dir;
    let depth_p1 = depth_texture.SampleLevel(point_sampler, uv_p1, 0).r;
    let depth_m1 = depth_texture.SampleLevel(point_sampler, uv_m1, 0).r;
    let ao_p1 = input_ao_texture.SampleLevel(point_sampler, uv_p1, 0).r;
    let ao_m1 = input_ao_texture.SampleLevel(point_sampler, uv_m1, 0).r;

    let center_w = 1.0f / center_depth;

    let in_bounds_p1 = all(uv_p1 >= 0.0f && uv_p1 <= 1.0f);
    let in_bounds_m1 = all(uv_m1 >= 0.0f && uv_m1 <= 1.0f);
    let valid_p1 = in_bounds_p1 && (depth_p1 > 1e-5f);
    let valid_m1 = in_bounds_m1 && (depth_m1 > 1e-5f);

    let w_p1 = 1.0f / select(valid_p1, depth_p1, 1.0f);
    let w_m1 = 1.0f / select(valid_m1, depth_m1, 1.0f);

    static const float sentinel = 1e20f;
    let dw_fwd = select(valid_p1, w_p1 - center_w, sentinel);
    let dw_bwd = select(valid_m1, center_w - w_m1, sentinel);
    let min_dw = select(abs(dw_fwd) < abs(dw_bwd), dw_fwd, dw_bwd);
    let dw = select(valid_p1 || valid_m1, min_dw, 0.0f);

    static const float log2e = 1.44269504f;
    let neg_inv_sigma_log2e = -log2e / max(gtao_blur_push_constants.depth_discontinuity_threshold * center_w, 1e-4f);

    // We've already fetched r0 and r1 taps, reuse these so we don't need to sample them again.
    var total_ao = center_ao * gaussian_kernel_weights[0];
    var total_weight = gaussian_kernel_weights[0];

    let w1 = gaussian_kernel_weights[1];
    accumulate_tap_w(ao_p1, w_p1, valid_p1, center_w, dw, neg_inv_sigma_log2e, 1.0f, w1, total_ao, total_weight);
    accumulate_tap_w(ao_m1, w_m1, valid_m1, center_w, dw, neg_inv_sigma_log2e, -1.0f, w1, total_ao, total_weight);

    // Accumulate taps on 2 to 4 radii
    [unroll]
    for (var r = 2; r <= 4; ++r) {
        let spatial_weight = gaussian_kernel_weights[r];
        let step_f = float(r);
        let offset = step_f * blur_dir;

        let uv_pos = input.uv + offset;
        let uv_neg = input.uv - offset;

        let d_pos = depth_texture.SampleLevel(point_sampler, uv_pos, 0).r;
        let d_neg = depth_texture.SampleLevel(point_sampler, uv_neg, 0).r;
        let ao_pos = input_ao_texture.SampleLevel(point_sampler, uv_pos, 0).r;
        let ao_neg = input_ao_texture.SampleLevel(point_sampler, uv_neg, 0).r;

        let in_bounds_pos = all(uv_pos >= 0.0f && uv_pos <= 1.0f);
        let in_bounds_neg = all(uv_neg >= 0.0f && uv_neg <= 1.0f);

        accumulate_tap(ao_pos, d_pos, in_bounds_pos, center_w, dw, neg_inv_sigma_log2e, +step_f, spatial_weight, total_ao, total_weight);
        accumulate_tap(ao_neg, d_neg, in_bounds_neg, center_w, dw, neg_inv_sigma_log2e, -step_f, spatial_weight, total_ao, total_weight);
    }

    return FragmentOutput(total_ao / total_weight);
}