import "../common/clusters";

struct BuildClusterGridPushConstants {
    uint64_t clusters_buffer_address;
    ClusterGridCreateInfo create_info;
};

[[vk::push_constant]]
ConstantBuffer<BuildClusterGridPushConstants> push_constants;

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    const uint tile_index = dispatch_thread_id.x;
    ClusterGridCreateInfo create_info = push_constants.create_info;

    const uint grid_cx = create_info.workgroup_count_tile_size_px.x;
    const uint grid_cy = create_info.workgroup_count_tile_size_px.y;
    const uint grid_cz = create_info.workgroup_count_tile_size_px.z;
    const uint total_clusters = grid_cx * grid_cy * grid_cz;

    if (tile_index >= total_clusters) {
        return;
    }

    ClusterBounds* clusters = (ClusterBounds*)push_constants.clusters_buffer_address;

    const uint tile_x = tile_index % grid_cx;
    const uint tile_y = (tile_index / grid_cx) % grid_cy;
    const uint tile_z = tile_index / (grid_cx * grid_cy);

    const float3 eye_position_view = float3(0.0);
    const float w = create_info.screen_bounds.x;
    const float h = create_info.screen_bounds.y;
    const float cx = float(grid_cx);
    const float cy = float(grid_cy);

    const float x0 = (float(tile_x) / cx) * w;
    const float x1 = (float(tile_x + 1) / cx) * w;
    const float y0 = (float(tile_y) / cy) * h;
    const float y1 = (float(tile_y + 1) / cy) * h;

    float3 p0_view = screen_to_view(float4(x0, y0, 1.0f, 1.0f), create_info);
    float3 p1_view = screen_to_view(float4(x1, y0, 1.0f, 1.0f), create_info);
    float3 p2_view = screen_to_view(float4(x0, y1, 1.0f, 1.0f), create_info);
    float3 p3_view = screen_to_view(float4(x1, y1, 1.0f, 1.0f), create_info);

    const float z_near = create_info.screen_bounds.z;
    const float z_far = create_info.screen_bounds.w;
    const float z_far_over_near = z_far / max(z_near, 1e-4);

    const float workgroup_size_z = float(grid_cz);
    const float tile_near = -z_near * pow(z_far_over_near, float(tile_z) / workgroup_size_z);
    const float tile_far = -z_near * pow(z_far_over_near, float(tile_z + 1) / workgroup_size_z);

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

    ClusterBounds bounds;
    bounds.min_corner = float4(min_point_aabb, 0.0);
    bounds.max_corner = float4(max_point_aabb, 0.0);
    clusters[tile_index] = bounds;
}
