#ifndef tempest_common_pulling_hlsl
#define tempest_common_pulling_hlsl

#include "consts.hlsl"
#include "mesh.hlsl"

float2 pull_vec2_f32(StructuredBuffer<uint> source, uint byte_offset)
{
    uint index = byte_offset / 4;
    uint v0 = source[index + 0];
    uint v1 = source[index + 1];
    
    float2 v = {
        asfloat(v0),
        asfloat(v1),
    };

    return v;
}

float3 pull_vec3_f32(StructuredBuffer<uint> source, uint byte_offset)
{
    uint index = byte_offset / 4;
    uint v0 = source[index + 0];
    uint v1 = source[index + 1];
    uint v2 = source[index + 2];
    
    float3 v = {
        asfloat(v0),
        asfloat(v1),
        asfloat(v2),
    };

    return v;
}

float4 pull_vec4_f32(StructuredBuffer<uint> source, uint byte_offset)
{
    uint index = byte_offset / 4;
    uint v0 = source[index + 0];
    uint v1 = source[index + 1];
    uint v2 = source[index + 2];
    uint v3 = source[index + 3];
    
    float4 v = {
        asfloat(v0),
        asfloat(v1),
        asfloat(v2),
        asfloat(v3),
    };

    return v;
}

bool has_component(uint offset)
{
    return offset != MAX_UINT;
}

Vertex pull_vertex(StructuredBuffer<uint> source, uint byte_offset, Mesh mesh, uint index)
{
    uint mesh_start_word = byte_offset / 4;
    uint vertex_id = source[mesh_start_word + mesh.index_offset / 4 + index];

    Vertex v;
    v.position = pull_vec3_f32(source, byte_offset + mesh.positions_offset + vertex_id * 4 * 3);

    uint interleave_start = byte_offset + mesh.interleave_offset + mesh.interleave_stride * vertex_id;

    v.uv0 = pull_vec2_f32(source, byte_offset + interleave_start + mesh.uv_offset);
    v.normal = pull_vec3_f32(source, byte_offset + interleave_start + mesh.normal_offset);
    v.tangent = has_component(mesh.tangent_offset) ? pull_vec3_f32(source, byte_offset + interleave_start + mesh.tangent_offset) : float3(0, 0, 0);
    v.bitangent = has_component(mesh.bitangent_offset) ? pull_vec3_f32(source, byte_offset + interleave_start + mesh.bitangent_offset) : float3(0, 0, 0);
    v.color = has_component(mesh.color_offset) ? pull_vec4_f32(source, byte_offset + interleave_start + mesh.color_offset) : float4(1.0, 0.0, 1.0, 1.0);

    return v;
}

#endif // tempest_common_pulling_hlsl