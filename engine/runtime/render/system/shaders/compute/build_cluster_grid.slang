import "../common/clusters";

struct BuildClusterGridPushConstants {
    uint64_t clusters_buffer_address;
    ClusterGridCreateInfo create_info;
};

[[vk::push_constant]]
ConstantBuffer<BuildClusterGridPushConstants> push_constants;

[shader("compute")]
[numthreads(1, 1, 1)]
void CSMain(uint3 work_group_id : SV_GroupID) {
    ClusterBounds* clusters = (ClusterBounds*)push_constants.clusters_buffer_address;
    ClusterGridCreateInfo create_info = push_constants.create_info;

    const float3 eye_position_view = float3(0.0);
    const uint tile_size_px = create_info.workgroup_count_tile_size_px.w;
    const uint tile_index = work_group_id.x + work_group_id.y * create_info.workgroup_count_tile_size_px.x + work_group_id.z * create_info.workgroup_count_tile_size_px.x * create_info.workgroup_count_tile_size_px.y;

    float4 max_point_screen_space = float4(float2(work_group_id.x + 1, work_group_id.y + 1) * tile_size_px, -1.0f, 1.0f);
    float4 min_point_screen_space = float4(work_group_id.xy * tile_size_px, -1.0f, 1.0f);

    float3 max_point_view_space = screen_to_view(max_point_screen_space, create_info);
    float3 min_point_view_space = screen_to_view(min_point_screen_space, create_info);

    const float z_near = create_info.screen_bounds.z;
    const float z_far = create_info.screen_bounds.w;
    const float z_far_over_near = z_far / max(z_near, 1e-4);

    const float workgroup_size_z = float(create_info.workgroup_count_tile_size_px.z);
    const float tile_near = -z_near * pow(z_far_over_near, float(work_group_id.z) / workgroup_size_z);
    const float tile_far = -z_near * pow(z_far_over_near, float(work_group_id.z + 1) / workgroup_size_z);

    const float3 min_point_near = line_intersection_to_z_plane(eye_position_view, min_point_view_space, tile_near);
    const float3 max_point_near = line_intersection_to_z_plane(eye_position_view, max_point_view_space, tile_near);
    const float3 min_point_far = line_intersection_to_z_plane(eye_position_view, min_point_view_space, tile_far);
    const float3 max_point_far = line_intersection_to_z_plane(eye_position_view, max_point_view_space, tile_far);

    const float3 min_point_aabb = min(min(min_point_near, min_point_far), min(max_point_near, max_point_far));
    const float3 max_point_aabb = max(max(max_point_near, max_point_far), max(min_point_near, min_point_far)); 

    ClusterBounds bounds;
    bounds.min_corner = float4(min_point_aabb, 0.0);
    bounds.max_corner = float4(max_point_aabb, 0.0);
    clusters[tile_index] = bounds;
}
