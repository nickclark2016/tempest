import "../common/tonemap";
import "../common/fsquad";

[vk_binding(0, 0)] Texture2D hdr_buffer;
[vk_binding(1, 0)] SamplerState linear_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
}

struct FragmentOutput {
    float4 color : SV_Target0;
}

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
    var hdr_color = hdr_buffer.Sample(linear_sampler, input.uv);
    hdr_color.rgb = hdr_color.rgb * exp2(1.0);
    output.color = float4(ACES(hdr_color.rgb), 1.0);
    return output;
}