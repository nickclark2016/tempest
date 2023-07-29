[[vk::binding(0, 0)]] Texture2D input_tex;
[[vk::binding(1, 0)]] SamplerState input_samp;

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv_coord : UVCOORD0;
};

static const float2 positions[4] = {
    float2(-1.0, 1.0),
    float2(1.0, 1.0),
    float2(1.0, -1.0),
    float2(-1.0, -1.0)
};

static const uint indices[6] = {
    0, 1, 2, 2, 3, 0
};

PSInput VSMain(uint vertex_id : SV_VERTEXID) {
    int index = indices[vertex_id];

    PSInput result;

    result.position = float4(positions[index], 0.0, 1.0);
    result.uv_coord = (positions[index] + 1.0) / 2.0;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input_tex.Sample(input_samp, input.uv_coord);
}