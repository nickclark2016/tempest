#ifndef TEMPEST_SHADERS_COMMON_CLUSTERS_SLANG
#define TEMPEST_SHADERS_COMMON_CLUSTERS_SLANG

struct ClusterGridCreateInfo {
    float4x4 inv_projection;
    float4 screen_bounds;
    uint4 workgroup_count_tile_size_px;
};

struct ClusterBounds {
    float4 min_corner;
    float4 max_corner;
};

struct LightGridRange {
    uint32_t offset;
    uint32_t count;
};

float4 clip_to_view(float4 clip, const ClusterGridCreateInfo create_info) {
    float4 view = mul(create_info.inv_projection, clip);
    return view / max(abs(view.w), 1e-6);
}

float3 screen_to_view(float4 screen_coord, const ClusterGridCreateInfo create_info) {
    float2 tex_coord_ndc = screen_coord.xy / create_info.screen_bounds.xy;
    float4 clip = float4(tex_coord_ndc * 2.0f - 1.0f, screen_coord.z, screen_coord.w);
    return clip_to_view(clip, create_info).xyz;
}

float3 line_intersection_to_z_plane(float3 eye, float3 point, float z) {
    const float3 normal = float3(0, 0, 1);
    const float3 eye_to_point = point - eye;
    const float denom = dot(normal, eye_to_point);
    const float t = (z - dot(normal, eye)) / (abs(denom) > 1e-6 ? denom : 1e-6);
    return eye + t * eye_to_point;
}

#endif // TEMPEST_SHADERS_COMMON_CLUSTERS_SLANG
