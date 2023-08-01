#ifndef tempest_common_mesh_hlsl
#define tempest_common_mesh_hlsl

struct Mesh {
    uint vertex_size;
    uint positions_offset;
    uint uv_offset;
    uint normal_offset;
    uint tangent_offset;
    uint bitangent_offset;
    uint color_offset;
    uint index_offset;
};

struct ObjectData {
    float4x4 model_matrix;
};

struct CameraData {
    float4x4 proj_matrix;
    float4x4 view_matrix;
    float4x4 view_proj_matrix;
};

#endif // tempest_common_mesh_hlsl