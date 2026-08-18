import "consts";
import "mesh";

float2 pull_float2_f32(uint* src, uint byte_offset) {
    uint index = byte_offset / 4;
    return {
        reinterpret<float>(src[index + 0]),
        reinterpret<float>(src[index + 1]),
    };
}

float3 pull_float3_f32(uint* src, uint byte_offset) {
    uint index = byte_offset / 4;
    return {
        reinterpret<float>(src[index + 0]),
        reinterpret<float>(src[index + 1]),
        reinterpret<float>(src[index + 2]),
    };
}

float4 pull_float4_f32(uint* src, uint byte_offset) {
    uint index = byte_offset / 4;
    return {
        reinterpret<float>(src[index + 0]),
        reinterpret<float>(src[index + 1]),
        reinterpret<float>(src[index + 2]),
        reinterpret<float>(src[index + 3]),
    };
}

bool has_attribute(uint attribute_offset) {
    return attribute_offset != MAX_UINT;
}

Vertex pull_vertex(Mesh* mesh, const uint vertex_id) {
    uint* src = (uint*)mesh.vertex_buffer_address;
    uint mesh_byte_offset = mesh.mesh_start_offset;

    Vertex v;
    v.position = pull_float3_f32(src, mesh_byte_offset + mesh.positions_offset + vertex_id * 12);

    uint interleave_offset = mesh_byte_offset + mesh.interleave_offset + mesh.interleave_stride * vertex_id;
    v.uv0 = pull_float2_f32(src, interleave_offset + mesh.uv_offset);
    v.normal = pull_float3_f32(src, interleave_offset + mesh.normal_offset);
    v.tangent = has_attribute(mesh.tangent_offset) ? pull_float4_f32(src, interleave_offset + mesh.tangent_offset) : float4(1.0, 0.0, 0.0, 1.0);
    v.color = has_attribute(mesh.color_offset) ? pull_float4_f32(src, interleave_offset + mesh.color_offset) : float4(1.0, 1.0, 1.0, 1.0);

    return v;
}
