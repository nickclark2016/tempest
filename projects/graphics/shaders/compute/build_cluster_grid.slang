import "../common/clusters";

[vk_binding(0, 0)] RWStructuredBuffer<ClusterBounds> clusters;

[shader("compute")]
[numthreads(1, 1, 1)]
void CSMain(uint3 work_group_id : SV_GroupID, [vk_push_constant] uniform ClusterGridCreateInfo create_info) {
    const float3 eye_position_view = float3(0.0);
    const uint tile_size_px = create_info.workgroup_count_tile_size_px.w;
    const uint tile_index = work_group_id.x + work_group_id.y * create_info.workgroup_count_tile_size_px.x + work_group_id.z * create_info.workgroup_count_tile_size_px.x * create_info.workgroup_count_tile_size_px.y;

    // Compute the bounds in screen space
    float4 max_point_screen_space = float4(float2(work_group_id.x + 1, work_group_id.y + 1) * tile_size_px, -1.0f, 1.0f); // Top Right
    float4 min_point_screen_space = float4(work_group_id.xy * tile_size_px, -1.0f, 1.0f); // Bottom Left

    // Convert screen space bounds to view space
    float3 max_point_view_space = screen_to_view(max_point_screen_space, create_info); // Top Right in view space
    float3 min_point_view_space = screen_to_view(min_point_screen_space, create_info); // Bottom Left in view space

    const float z_near = create_info.screen_bounds.z; // Near plane Z value
    const float z_far = create_info.screen_bounds.w; // Far plane Z value
    const float z_far_over_near = z_far / z_near; // Precompute this value for the formula below

    const float workground_size_z = create_info.workgroup_count_tile_size_px.z;
    const float tile_near = -z_near * pow(z_far_over_near, work_group_id.z / workground_size_z); // Calculate the near plane for this tile based on its Z index
    const float tile_far = -z_near * pow(z_far_over_near, (work_group_id.z + 1) / workground_size_z); // Calculate the far plane for this tile based on the next Z index

    const float3 min_point_near = line_intersection_to_z_plane(eye_position_view, min_point_view_space, tile_near); // Calculate the intersection of the near plane with the min point in view space
    const float3 max_point_near = line_intersection_to_z_plane(eye_position_view, max_point_view_space, tile_near); // Calculate the intersection of the near plane with the max point in view space
    const float3 min_point_far = line_intersection_to_z_plane(eye_position_view, min_point_view_space, tile_far); // Calculate the intersection of the far plane with the min point in view space
    const float3 max_point_far = line_intersection_to_z_plane(eye_position_view, max_point_view_space, tile_far); // Calculate the intersection of the far plane with the max point in view space

    // Calculate the axis-aligned bounding box (AABB) for the tile in view space
    const float3 min_point_aabb = min(min(min_point_near, min_point_far), min(max_point_near, max_point_far));
    const float3 max_point_aabb = max(max(max_point_near, max_point_far), max(min_point_near, min_point_far)); 

    clusters[tile_index] = ClusterBounds(float4(min_point_aabb, 0.0), float4(max_point_aabb, 0.0));
}
