import "../common/fsquad";
import "../common/material";
import "../common/mboit";

struct OitBlendPushConstants {
    int32_t accumulation_texture_index;
    int32_t zeroth_moment_storage_index;
    int32_t linear_sampler_index;
    int32_t padding;
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
    if (push_constants.accumulation_texture_index < 0 || push_constants.zeroth_moment_storage_index < 0) {
        discard;
    }
    
    SamplerState linear_sampler = bindless_samplers[push_constants.linear_sampler_index];
    Texture2D accum_tex = bindless_textures[push_constants.accumulation_texture_index];
    float4 accum = accum_tex.Sample(linear_sampler, input.uv);
    
    int2 addr = int2(input.position.xy);
    float b0 = bindless_storage_float[push_constants.zeroth_moment_storage_index].Load(addr);
    
    if (b0 < 0.00100050033 || accum.a <= 1e-4) {
        discard;
    }
    
    float transmittance = exp(-b0);
    if (isinf(b0)) {
        transmittance = 1e-7;
    }
    
    float4 color = float4(accum.rgb / max(accum.a, 1e-6), 1.0 - transmittance);
    return FragmentOutput(color);
}
