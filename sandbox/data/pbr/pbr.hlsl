#include "../common/pulling.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<uint> mesh_data;

struct PSInput {
    float4 position : SV_POSITION;
};

PSInput VSMain(uint vertex_id : SV_VERTEXID) {
    float3 position = pull_vec3_f32(mesh_data, vertex_id * 3);
    
    PSInput result;
    result.position = float4(position, 1.0);

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return float4(1.0, 1.0, 0.0, 1.0);
}