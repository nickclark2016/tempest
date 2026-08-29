#ifndef TEMPEST_SHADERS_COMMON_CLUSTERS_SLANG
#define TEMPEST_SHADERS_COMMON_CLUSTERS_SLANG

import "scene";

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
    float denom = abs(view.w) > 1e-6 ? view.w : 1e-6;
    return view / denom;
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

uint compute_cluster_z_index(float view_z, float z_near, float z_far, uint z_slice_count) {
    view_z = min(view_z, -z_near);
    float z_far_over_near = z_far / max(z_near, 1e-4);
    float log_factor = log(-view_z / max(z_near, 1e-4)) / log(z_far_over_near);
    uint z_index = uint(log_factor * z_slice_count);
    return min(z_index, z_slice_count - 1);
}

Optional<uint> get_cluster_index(float3 view_pos, float2 screen_coord, SceneGlobals* scene) {
    float z_near = scene.cluster_depth_params.x;
    float z_far = scene.cluster_depth_params.y;
    if (view_pos.z > -z_near || view_pos.z < -z_far) {
        return none;
    }

    uint4 cluster_counts_tile_size = scene.cluster_counts_tile_size;
    uint tile_size_px = cluster_counts_tile_size.w;

    uint tile_x = min(uint(screen_coord.x / float(tile_size_px)), cluster_counts_tile_size.x - 1);
    uint tile_y = min(uint(screen_coord.y / float(tile_size_px)), cluster_counts_tile_size.y - 1);
    uint tile_z = compute_cluster_z_index(view_pos.z, z_near, z_far, cluster_counts_tile_size.z);

    return tile_x + tile_y * cluster_counts_tile_size.x + tile_z * cluster_counts_tile_size.x * cluster_counts_tile_size.y;
}

Optional<uint> get_cluster_index_from_view_pos(float3 view_pos, float2 screen_coord, SceneGlobals* scene) {
    return get_cluster_index(view_pos, screen_coord, scene);
}

#endif // TEMPEST_SHADERS_COMMON_CLUSTERS_SLANG
