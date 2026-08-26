import "../common/clusters";

[vk_binding(0, 0)] RWStructuredBuffer<ClusterBounds> clusters;

[shader("compute")]
[numthreads(1, 1, 1)]
void CSMain(uint3 work_group_id : SV_GroupID, uniform ClusterGridCreateInfo create_info) {
    const float3 eye_position_view = float3(0.0);
    const uint tile_size_px = create_info.workgroup_count_tile_size_px.w;
    const uint tile_index = work_group_id.x + work_group_id.y * create_info.workgroup_count_tile_size_px.x + work_group_id.z * create_info.workgroup_count_tile_size_px.x * create_info.workgroup_count_tile_size_px.y;

    float3 p0_view = screen_to_view(float4(float2(work_group_id.x,     work_group_id.y)     * tile_size_px, -1.0f, 1.0f), create_info);
    float3 p1_view = screen_to_view(float4(float2(work_group_id.x + 1, work_group_id.y)     * tile_size_px, -1.0f, 1.0f), create_info);
    float3 p2_view = screen_to_view(float4(float2(work_group_id.x,     work_group_id.y + 1) * tile_size_px, -1.0f, 1.0f), create_info);
    float3 p3_view = screen_to_view(float4(float2(work_group_id.x + 1, work_group_id.y + 1) * tile_size_px, -1.0f, 1.0f), create_info);

    const float z_near = create_info.screen_bounds.z;
    const float z_far = create_info.screen_bounds.w;
    const float z_far_over_near = z_far / max(z_near, 1e-4);

    const float workground_size_z = create_info.workgroup_count_tile_size_px.z;
    const float tile_near = -z_near * pow(z_far_over_near, work_group_id.z / workground_size_z);
    const float tile_far = -z_near * pow(z_far_over_near, (work_group_id.z + 1) / workground_size_z);

    const float3 p0_near = line_intersection_to_z_plane(eye_position_view, p0_view, tile_near);
    const float3 p1_near = line_intersection_to_z_plane(eye_position_view, p1_view, tile_near);
    const float3 p2_near = line_intersection_to_z_plane(eye_position_view, p2_view, tile_near);
    const float3 p3_near = line_intersection_to_z_plane(eye_position_view, p3_view, tile_near);

    const float3 p0_far = line_intersection_to_z_plane(eye_position_view, p0_view, tile_far);
    const float3 p1_far = line_intersection_to_z_plane(eye_position_view, p1_view, tile_far);
    const float3 p2_far = line_intersection_to_z_plane(eye_position_view, p2_view, tile_far);
    const float3 p3_far = line_intersection_to_z_plane(eye_position_view, p3_view, tile_far);

    const float3 min_point_aabb = min(min(min(p0_near, p1_near), min(p2_near, p3_near)),
                                      min(min(p0_far,  p1_far),  min(p2_far,  p3_far)));
    const float3 max_point_aabb = max(max(max(p0_near, p1_near), max(p2_near, p3_near)),
                                      max(max(p0_far,  p1_far),  max(p2_far,  p3_far)));

    clusters[tile_index] = ClusterBounds(float4(min_point_aabb, 0.0), float4(max_point_aabb, 0.0));
}
