#ifndef tempest_vertex_pull_slang
#define tempest_vertex_pull_slang

#include "consts.slang"
#include "mesh.slang"

float2 pull_float2_f32(StructuredBuffer<uint> src, uint byte_offset) {
    uint index = byte_offset / 4;
    uint v0 = src[index + 0];
    uint v1 = src[index + 1];

    return {
        reinterpret<float>(v0),
        reinterpret<float>(v1),
    };
}

float3 pull_float3_f32(StructuredBuffer<uint> src, uint byte_offset) {
    uint index = byte_offset / 4;
    uint v0 = src[index + 0];
    uint v1 = src[index + 1];
    uint v2 = src[index + 2];

    return {
        reinterpret<float>(v0),
        reinterpret<float>(v1),
        reinterpret<float>(v2),
    };
}

float4 pull_float4_f32(StructuredBuffer<uint> src, uint byte_offset) {
    uint index = byte_offset / 4;
    uint v0 = src[index + 0];
    uint v1 = src[index + 1];
    uint v2 = src[index + 2];
    uint v3 = src[index + 3];


    return {
        reinterpret<float>(v0),
        reinterpret<float>(v1),
        reinterpret<float>(v2),
        reinterpret<float>(v3)
    };
}

bool has_attribute(uint attribute_offset) {
    return attribute_offset != MAX_UINT;
}

Vertex pull_vertex(StructuredBuffer<uint> src, const Mesh mesh, const uint vertex_id) {
    uint mesh_byte_offset = mesh.mesh_start_offset;
    
    Vertex v;
    v.position = pull_float3_f32(src, mesh_byte_offset + mesh.positions_offset + vertex_id * 4 * 3);
    
    uint interleave_offset = mesh_byte_offset + mesh.interleave_offset + mesh.interleave_stride * vertex_id;
    v.uv0 = pull_float2_f32(src, interleave_offset + mesh.uv_offset);
    v.normal = pull_float3_f32(src, interleave_offset + mesh.normal_offset);
    v.tangent = has_attribute(mesh.tangent_offset) ? pull_float4_f32(src, interleave_offset + mesh.tangent_offset) : float4(1, 0, 0, 1);
    v.color = has_attribute(mesh.color_offset) ? pull_float4_f32(src, interleave_offset + mesh.color_offset) : float4(1, 1, 1, 1);

    return v;
}

#endif // tempest_vertex_pull_slang