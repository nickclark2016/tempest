// mesh.slang

struct Mesh {
    uint mesh_start_offset;
    uint positions_offset;
    uint interleave_offset;
    uint interleave_stride;
    uint uv_offset;
    uint normal_offset;
    uint tangent_offset;
    uint color_offset;
    uint index_offset;
    uint index_count;
};

struct ObjectData {
    float4x4 model_matrix;
    float4x4 transpose_inv_model_matrix;
    float4x4 prev_model_matrix;
    uint mesh_id;
    uint material_id;
    uint parent_id;
    uint self_id;
};

struct Vertex {
    float3 position;
    float2 uv0;
    float3 normal;
    float4 tangent;
    float4 color;
};