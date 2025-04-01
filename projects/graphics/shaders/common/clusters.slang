struct ClusterGridCreateInfo {
    float4x4 inv_projection; // Inverse projection matrix to convert screen space to view space
    float4 screen_bounds; // (width, height, min_z, max_z)
    uint4 workgroup_count_tile_size_px; // (workgroup_count_x, workgroup_count_y, workgroup_count_z, tile_size_px)
}

struct ClusterBounds {
    float4 min_corner;
    float4 max_corner;
}

struct LightGridRange {
    uint offset;
    uint count;
}

float4 clip_to_view(float4 clip, const ClusterGridCreateInfo create_info) {
    float4 view = mul(create_info.inv_projection, clip);
    view /= view.w;
    return view;
}

float3 screen_to_view(float4 screen_coord, const ClusterGridCreateInfo create_info) {
    float2 tex_coord_ndc = screen_coord.xy / create_info.screen_bounds.xy;
    float4 clip = float4(tex_coord_ndc * 2.0f - 1.0f, screen_coord.z, screen_coord.w);
    return clip_to_view(clip, create_info).xyz;
}

float3 line_intersection_to_z_plane(float3 eye, float3 point, float z) {
    const float3 normal = float3(0, 0, 1);
    const float3 eye_to_point = point - eye;
    const float t = (z - dot(normal, eye)) / dot(normal, eye_to_point);
    return eye + t * eye_to_point;
}
