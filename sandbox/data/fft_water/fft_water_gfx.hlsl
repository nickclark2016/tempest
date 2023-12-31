#include "../common/camera.hlsl"
#include "../common/consts.hlsl"
#include "../common/lighting.hlsl"
#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

#define MESH_VERTEX_QUAD_X 1024
#define MESH_VERTEX_QUAD_Z 1024

[[vk::binding(0, 0)]]
cbuffer Constants
{
    CameraData camera_data;
    DirectionalLight sun;
    float4 tiling_factors;
    float displacement_depth_attenuation;
};

[[vk::binding(1, 0)]] StructuredBuffer<uint> vertex_pull_buffer;
[[vk::binding(2, 0)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(3, 0)]] StructuredBuffer<ObjectData> instances;
[[vk::binding(4, 0)]] Texture2DArray<float4> displacement_textures;
[[vk::binding(5, 0)]] Texture2DArray<float2> slope_textures;
[[vk::binding(6, 0)]] SamplerState texture_sampler; 

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TexCoord0;
    float4 world_pos : Position;
    float depth : Depth;
};

float4x4 inverse(float4x4 m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}

VSOutput VSMain(uint index_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instances[instance_id].mesh_id], index_id);
    VSOutput output;
    output.uv = v.uv0;
    float4x4 model_matrix = instances[instance_id].model_matrix;
    output.world_pos = mul(model_matrix, float4(v.position, 1.0f));
    return output;
}

// VSOutput VSMain(uint index_id : SV_VertexID, uint instance_id : SV_InstanceID)
// {   
//     Vertex v = pull_vertex(vertex_pull_buffer, meshes[instances[instance_id].mesh_id], index_id);

//     VSOutput output;
    
//     float4x4 model_matrix = instances[instance_id].model_matrix;

//     output.world_pos = mul(model_matrix, float4(v.position, 1.0));
    
//     float4 displacement_0 = displacement_textures.SampleLevel(texture_sampler, float3(v.uv0 * tiling_factors.x, 0), 0);
//     float4 displacement_1 = displacement_textures.SampleLevel(texture_sampler, float3((v.uv0 - 0.5f) * tiling_factors.y, 1), 0);
//     float4 displacement_2 = displacement_textures.SampleLevel(texture_sampler, float3((v.uv0 - 1.125f) * tiling_factors.y, 2), 0);
//     float4 displacement_3 = displacement_textures.SampleLevel(texture_sampler, float3((v.uv0 - 1.25f) * tiling_factors.y, 3), 0);
//     float4 total_displacement = displacement_0 + displacement_1 + displacement_2 + displacement_3;

//     total_displacement = lerp(0.0f, displacement_0, pow(saturate(output.depth), displacement_depth_attenuation));

//     v.position += mul(inverse(model_matrix), float4(total_displacement.xyz, 1.0f)).xyz;
    
//     output.uv = v.uv0;
//     output.depth = 1 - output.world_pos.z / output.world_pos.w;
//     output.pos = mul(camera_data.proj_matrix, mul(camera_data.view_matrix, mul(model_matrix, float4(v.position, 1.0f))));

//     return output;
// }

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}