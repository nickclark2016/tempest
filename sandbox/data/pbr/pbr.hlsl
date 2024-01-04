#include "../common/camera.hlsl"
#include "../common/lighting.hlsl"
#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

[[vk::binding(0, 0)]]
cbuffer Constants
{
    CameraData camera_data;
    DirectionalLight sun;
};

[[vk::binding(1, 0)]]
StructuredBuffer<PointLight> point_lights;

[[vk::binding(2, 0)]] StructuredBuffer<uint> vertex_pull_buffer;
[[vk::binding(3, 0)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(4, 0)]] StructuredBuffer<ObjectData> objects;
[[vk::binding(5, 0)]] StructuredBuffer<Material> materials;
[[vk::binding(6, 0)]] SamplerState bindless_sampler;
[[vk::binding(7, 0)]] Texture2D bindless_textures[];

struct PSInput {
    float4 position : SV_Position;
    float3 vpos : Position;
    float2 uv : UV0;
    uint material_id : Material;
};

PSInput VSMain(uint index_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    Vertex v = pull_vertex(vertex_pull_buffer, meshes[objects[instance_id].mesh_id], index_id);
    ObjectData instance = objects[instance_id];

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(camera_data.view_matrix, world_pos);

    PSInput result;
    result.position = mul(camera_data.proj_matrix, view_pos);
    result.vpos = v.position;
    result.uv = v.uv0;
    result.material_id = instance.material_id;

    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    Material mat = materials[input.material_id];
    if (mat.base_texture == MAX_UINT)
    {
        return float4(0, 0, 0, 1);
    }
    float4 base = bindless_textures[mat.base_texture].Sample(bindless_sampler, input.uv);
    if (base.a < 0.5) {
        discard;
    }
    base.a = 1.0f;
    return base;
}