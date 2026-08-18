import "../common/fsquad";
import "../common/material";

struct SSAOBlurPushConstants {
    int32_t ssao_texture_index;
    int32_t depth_texture_index;
    int32_t point_sampler_index;
    int32_t linear_sampler_index;
    float2 inv_screen_size;
    float depth_threshold;
    float _pad;
};

[[vk::push_constant]]
ConstantBuffer<SSAOBlurPushConstants> push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FragmentOutput {
    float visibility : SV_Target0;
};

[shader("vertex")]
VSOutput VSMain(uint id : SV_VertexID) {
    let vtx = generate_quad_vertex(id);
    return VSOutput(
        vtx.position,
        vtx.uv
    );
}

[shader("fragment")]
FragmentOutput FSMain(VSOutput vs_out) {
    FragmentOutput output;
    if (push_constants.ssao_texture_index < 0) {
        output.visibility = 1.0;
        return output;
    }

    SamplerState point_sampler = bindless_samplers[push_constants.point_sampler_index];
    Texture2D ssao_tex = bindless_textures[push_constants.ssao_texture_index];

    float center_depth = 0.0;
    bool has_depth = push_constants.depth_texture_index >= 0;
    if (has_depth) {
        Texture2D depth_tex = bindless_textures[push_constants.depth_texture_index];
        center_depth = depth_tex.Sample(point_sampler, vs_out.uv).r;
    }

    float center_linear_z = 1.0 / max(center_depth, 1e-4);

    float total_weight = 0.0;
    float total_ssao = 0.0;
    float threshold = push_constants.depth_threshold > 0.0 ? push_constants.depth_threshold : 0.05f;

    for (int y = -2; y <= 2; ++y) {
        for (int x = -2; x <= 2; ++x) {
            float2 offset = float2(float(x), float(y)) * push_constants.inv_screen_size;
            float2 sample_uv = vs_out.uv + offset;

            if (sample_uv.x < 0.0 || sample_uv.x > 1.0 || sample_uv.y < 0.0 || sample_uv.y > 1.0) {
                continue;
            }

            float sample_ssao = ssao_tex.Sample(point_sampler, sample_uv).r;
            float spatial_weight = exp(-float(x * x + y * y) / 4.0f);
            float weight = spatial_weight;

            if (has_depth) {
                Texture2D depth_tex = bindless_textures[push_constants.depth_texture_index];
                float sample_depth = depth_tex.Sample(point_sampler, sample_uv).r;
                float sample_linear_z = 1.0 / max(sample_depth, 1e-4);
                float depth_diff = abs(center_linear_z - sample_linear_z);
                float depth_weight = exp(-depth_diff / max(threshold, 1e-4));
                weight *= depth_weight;
            }

            total_ssao += sample_ssao * weight;
            total_weight += weight;
        }
    }

    output.visibility = total_ssao / max(total_weight, 1e-4);
    return output;
}
