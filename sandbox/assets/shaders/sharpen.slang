[[vk::binding(0, 0)]] Texture2D<float4> input;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : UV;
};

[shader("vertex")]
VertexOutput VSMain(uint index_id : SV_VertexID) {
    uint index = index_id;
    float2 uv = float2((index << 1) & 2, index & 2);

    VertexOutput result;
    result.position = float4(uv * 2 - 1, 0, 1);
    result.uv = float2(uv.x, 1 - uv.y);
    return result;
}

[shader("fragment")]
float4 FSMain(VertexOutput vs_out) {
    float amount = 0.1;

    float2 frag_coord = vs_out.uv;
    
    uint width, height, levels;
    input.GetDimensions(0, width, height, levels);

    uint2 tex_coords = uint2(frag_coord * float2(width, height));

    float neighbor = amount * -1.0;
    float center = amount * 4.0 + 1.0;

    const uint2 neighbors[4] = {
        uint2(1, 0),
        uint2(-1, 0),
        uint2(0, 1),
        uint2(0, -1)
    };

    float4 sum = 0;
    for (uint i = 0; i < 4; i++) {
        uint2 coord = tex_coords + neighbors[i];
        sum += input.Load(int3(coord, 0)) * neighbor;
    }

    sum += input.Load(int3(tex_coords, 0)) * center;

    return sum;
}