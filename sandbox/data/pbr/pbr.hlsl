#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<uint> mesh_data;
[[vk::binding(1, 0)]] StructuredBuffer<ObjectData> object_data;
[[vk::binding(2, 0)]] cbuffer SceneData {
    CameraData camera;
};

struct PSInput {
    float4 pixel_position : SV_POSITION;
    float4 world_position : POSITION;
};

PSInput VSMain(uint vertex_id : SV_VERTEXID, uint instance_id : SV_INSTANCEID) {
    uint position_offset = vertex_id * 3;

    float4 position = float4(pull_vec3_f32(mesh_data, vertex_id * 3 * 4), 1.0);
    float4x4 model_matrix = object_data[instance_id].model_matrix;
    float4x4 view_proj_matrix = camera.view_proj_matrix;

    float4 world_position = mul(model_matrix, position);
    float4 pixel_position = mul(view_proj_matrix, world_position);

    PSInput result;
    result.world_position = world_position;
    result.pixel_position = pixel_position;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return float4(1.0, 1.0, 0.0, 1.0);
}