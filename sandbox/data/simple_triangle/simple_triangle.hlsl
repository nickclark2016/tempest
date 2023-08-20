#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<uint> mesh_data;
[[vk::binding(1, 0)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> object_data;
[[vk::binding(3, 0)]] cbuffer SceneData {
    CameraData camera;
};

struct PSInput {
    float4 pixel_position : SV_POSITION;
    float4 world_position : POSITION;
    float4 color : COLOR;
};

PSInput VSMain(uint index_id : SV_VERTEXID, uint instance_id : SV_INSTANCEID) {
    ObjectData object = object_data[instance_id];
    Mesh mesh = meshes[object.mesh_id];

    Vertex v = pull_vertex(mesh_data, mesh.mesh_start_offset, mesh, index_id);
    float4x4 model_matrix = object.model_matrix;
    float4x4 view_proj_matrix = camera.view_proj_matrix;

    float4 world_position = mul(model_matrix, float4(v.position, 1.0));
    float4 pixel_position = mul(view_proj_matrix, world_position);

    PSInput result;
    result.world_position = world_position;
    result.pixel_position = pixel_position;
    result.color = v.color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}