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

#endif // tempest_common_mesh_hlsl