#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

[[vk::binding(0, 0)]] StructuredBuffer<uint> mesh_data;
[[vk::binding(1, 0)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> object_data;
[[vk::binding(4, 0)]] cbuffer SceneData {
    CameraData camera;
};

struct PSInput {
    float4 px_position : SV_POSITION;
    nointerpolation uint object_id : OBJECT_ID;
    nointerpolation uint triangle_id : TRIANGLE_ID;
};

PSInput VSMain(uint index_id : SV_VERTEXID, uint instance_id : SV_INSTANCEID) {
    ObjectData object = object_data[instance_id];
    Mesh mesh = meshes[object.mesh_id];

    Vertex v = pull_vertex(mesh_data, mesh, index_id);
    
    float4x4 model_matrix = object.model_matrix;
    float4x4 view_proj_matrix = camera.view_proj_matrix;

    float4 world_position = mul(model_matrix, float4(v.position, 1.0));
    float4 pixel_position = mul(view_proj_matrix, world_position);

    PSInput result;
    result.px_position = pixel_position;
    result.object_id = instance_id;
    result.triangle_id = index_id / 3;

    return result;
}

uint2 PSMain(PSInput input) : SV_TARGET {
    return uint2(input.object_id, input.triangle_id);
}