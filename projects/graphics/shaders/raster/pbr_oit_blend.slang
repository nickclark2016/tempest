import "../common/fsquad";

struct VertexOutput {
    float4 position         : SV_Position;
    float2 uv               : UV0;
};

[shader("vertex")]
VertexOutput VSMain(uint index_id : SV_VertexID) {
    var vertex = generate_quad_vertex(index_id);
    var output = VertexOutput(vertex.position, vertex.uv);
    return output;
}

struct FragmentOutput {
    float4 color            : SV_Target0;
};

[vk_binding(0, 0)] RWTexture2DArray<half4> moments;
[vk_binding(1, 0)] RWTexture2D<float> zeroth_moment;
[vk_binding(2, 0)] Texture2D transparency_accumulator;
[vk_binding(3, 0)] SamplerState linear_sampler;

[shader("fragment")]
FragmentOutput FSMain(VertexOutput input) {
    float4 transparency = transparency_accumulator.Sample(linear_sampler, input.uv);
    
    var addr = int2(input.position.xy);
    var b0 = zeroth_moment.Load(addr);

    if (b0 < 0.00100050033h) {
        discard;
    }

    float transmittance = exp(-b0);
    if (isinf(b0)) {
        transmittance = 1e7;
    }

    var color = float4(transparency.rgb / transparency.a, 1.0f - transmittance);

    return FragmentOutput(color);
}