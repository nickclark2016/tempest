import "../common/clusters";
import "../common/lighting";
import "../common/scene";

struct LightPayload {
    float4 color_intensity;
    float4 position_falloff;
    float4 direction_angle;
    uint32_t type;
    uint32_t enabled;
    uint32_t padding[2];
};

struct LightCullingPushConstants {
    uint64_t scene_constants_address;
    uint64_t clusters_address;
    uint64_t lights_address;
    uint64_t light_indices_address;
    uint64_t light_grid_address;
    uint64_t global_count_address;
    uint4 workgroup_count_tile_size_px;
    uint32_t light_count;
    uint32_t padding[3];
};

[[vk::push_constant]]
ConstantBuffer<LightCullingPushConstants> push_constants;

groupshared uint shared_lights[16 * 9 * 4];

float square_distance_point_aabb(float3 point, ClusterBounds bounds) {
    float sq_distance = 0.0;
    for (uint i = 0; i < 3; ++i) {
        float v = point[i];
        if (v < bounds.min_corner[i]) {
            sq_distance += (bounds.min_corner[i] - v) * (bounds.min_corner[i] - v);
        }
        if (v > bounds.max_corner[i]) {
            sq_distance += (v - bounds.max_corner[i]) * (v - bounds.max_corner[i]);
        }
    }
    return sq_distance;
}

[shader("compute")]
[numthreads(16, 9, 4)]
void CSMain(uint invocation_id : SV_GroupIndex, uint3 group_id : SV_GroupID) {
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    ClusterBounds* clusters = (ClusterBounds*)push_constants.clusters_address;
    LightPayload* lights = (LightPayload*)push_constants.lights_address;
    uint* global_light_index_list = (uint*)push_constants.light_indices_address;
    LightGridRange* light_grid = (LightGridRange*)push_constants.light_grid_address;
    uint* global_index_count = (uint*)push_constants.global_count_address;

    uint thread_count = push_constants.workgroup_count_tile_size_px.x *
                        push_constants.workgroup_count_tile_size_px.y *
                        push_constants.workgroup_count_tile_size_px.z;
    uint light_count = push_constants.light_count;
    uint num_batches = (light_count + thread_count - 1) / thread_count;
    uint tile_index = invocation_id + thread_count * group_id.z;

    ClusterBounds bounds = clusters[tile_index];

    uint visible_light_count = 0;
    uint visible_light_indices[128];

    for (uint batch = 0; batch < num_batches && visible_light_count < 128; ++batch) {
        uint light_index = batch * thread_count + invocation_id;
        shared_lights[invocation_id] = min(light_index, light_count > 0 ? light_count - 1 : 0);
        GroupMemoryBarrierWithGroupSync();

        for (uint light = 0; light < thread_count && visible_light_count < 128; ++light) {
            uint global_idx = batch * thread_count + light;
            if (global_idx < light_count) {
                LightPayload l = lights[shared_lights[light]];
                if (l.enabled != 0) {
                    float range = l.position_falloff.w;
                    float sq_range = range * range;
                    float3 center = l.position_falloff.xyz;
                    float3 view_pos = mul(scene.camera.view, float4(center, 1.0)).xyz;
                    if (square_distance_point_aabb(view_pos, bounds) <= sq_range) {
                        visible_light_indices[visible_light_count] = global_idx;
                        visible_light_count++;
                    }
                }
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    uint offset = 0;
    InterlockedAdd(global_index_count[0], visible_light_count, offset);

    for (uint i = 0; i < visible_light_count; ++i) {
        global_light_index_list[offset + i] = visible_light_indices[i];
    }

    LightGridRange range;
    range.offset = offset;
    range.count = visible_light_count;
    light_grid[tile_index] = range;
}
