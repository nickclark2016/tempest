import "../common/camera";
import "../common/fsquad";
import "../common/material";

[vk_binding(0, 0)] Texture2D ssao_buffer;
[vk_binding(1, 0)] SamplerState point_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
}

struct FragmentOutput {
    float visibility : SV_Target0;
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
FragmentOutput FSMain(VSOutput vs_out) {
    FragmentOutput output;

    float blur_factor = 0.0f;

    uint width, height;
    ssao_buffer.GetDimensions(width, height);
    
    let tx_size = 1.0 / float2(width, height);
    var sample_count = 0.0f;

    for (int t = -2; t <= 2; ++t) {
        for (int s = -2; s <= 2; ++s) {
            let offset = float2(s, t) * tx_size;
            let sample_pos = vs_out.uv + offset;

            if (any(sample_pos < 0.0f) || any(sample_pos > 1.0f)) {
                continue;
            }

            blur_factor += ssao_buffer.Sample(point_sampler, sample_pos).r;
            sample_count += 1.0f;
        }
    }

    // Output the visibility value
    output.visibility = blur_factor / sample_count;

    return output;
}