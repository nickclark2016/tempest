import "../common/fsquad";
import "../common/material";
import "../common/tonemap";

struct TonemapPushConstants {
    int32_t hdr_texture_index;
    int32_t sampler_index;
    float exposure;
    float _pad;
};

[[vk::push_constant]]
ConstantBuffer<TonemapPushConstants> push_constants;

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
    float4 hdr_color = float4(0.0, 0.0, 0.0, 1.0);
    if (push_constants.hdr_texture_index >= 0) {
        hdr_color = bindless_textures[push_constants.hdr_texture_index].Sample(
            bindless_samplers[push_constants.sampler_index], input.uv);
    }
    hdr_color.rgb = hdr_color.rgb * exp2(push_constants.exposure);

    // Monochrome dither using Interleaved Gradient Noise to break up 8-bit quantization gradients
    float3 magic = float3(0.06711056, 0.00583715, 52.9829189);
    float ign = frac(magic.z * frac(dot(input.position.xy, magic.xy)));
    float dither = (ign - 0.5) / 255.0;

    output.color = float4(saturate(ACES(hdr_color.rgb) + dither), 1.0);
    return output;
}
