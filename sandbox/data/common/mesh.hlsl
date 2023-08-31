#ifndef tempest_common_mesh_hlsl
#define tempest_common_mesh_hlsl

struct Mesh {
    uint mesh_start_offset;
    uint positions_offset;
    uint interleave_offset;
    uint interleave_stride;
    uint uv_offset;
    uint normal_offset;
    uint tangent_offset;
    uint bitangent_offset;
    uint color_offset;
    uint index_offset;
    uint index_count;
};

struct ObjectData {
    float4x4 model_matrix;
    uint mesh_id;
};

struct CameraData {
    float4x4 proj_matrix;
    float4x4 view_matrix;
    float4x4 view_proj_matrix;
};

struct Vertex {
    float3 position;
    float2 uv0;
    float3 normal;
    float3 tangent;
    float3 bitangent;
    float4 color;
};

#endif // tempest_common_mesh_hlsl