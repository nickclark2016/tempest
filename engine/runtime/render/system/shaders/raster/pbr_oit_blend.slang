import "../common/fsquad";
import "../common/material";

struct OitBlendPushConstants {
    int32_t accumulation_texture_index;
    int32_t linear_sampler_index;
    float padding[2];
};

[[vk::push_constant]]
ConstantBuffer<OitBlendPushConstants> push_constants;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

struct FragmentOutput {
    float4 color : SV_Target0;
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
FragmentOutput FSMain(VSOutput input) {
    FragmentOutput output;
    if (push_constants.accumulation_texture_index < 0) {
        discard;
    }

    SamplerState linear_sampler = bindless_samplers[push_constants.linear_sampler_index];
    Texture2D accum_tex = bindless_textures[push_constants.accumulation_texture_index];

    float4 accum = accum_tex.Sample(linear_sampler, input.uv);
    if (accum.a <= 1e-4) {
        discard;
    }

    output.color = accum;
    return output;
}
