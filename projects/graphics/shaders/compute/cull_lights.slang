import "../common/clusters";
import "../common/lighting";
import "../common/scene";

[vk_binding(0, 0)] ConstantBuffer<SceneGlobals, Std430DataLayout> scene;
[vk_binding(1, 0)] RWStructuredBuffer<ClusterBounds> clusters;
[vk_binding(2, 0)] StructuredBuffer<Light> lights;
[vk_binding(3, 0)] RWStructuredBuffer<uint> global_light_index_list;
[vk_binding(4, 0)] RWStructuredBuffer<LightGridRange> light_grid;
[vk_binding(5, 0)] RWStructuredBuffer<uint> global_index_count;

groupshared Light shared_lights[16 * 9 * 4];

float square_distance_point_aabb(float3 point, uint tile) {
    float sq_distance = 0.0;
    ClusterBounds bounds = clusters[tile];
    clusters[tile].max_corner.w = tile;

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

bool test_light_in_tile(uint light_index, uint tile_index) {
    float range = shared_lights[light_index].position_falloff.w;
    float sq_range = range * range;
    float3 center = shared_lights[light_index].position_falloff.xyz;
    float3 view_pos = mul(scene.camera.view, float4(center, 1.0)).xyz;

    float sq_distance = square_distance_point_aabb(view_pos, tile_index);
    return sq_distance <= sq_range;
}

struct PushConstants {
    ClusterGridCreateInfo create_info;
    uint light_count;
}

[shader("compute")]
[numthreads(16, 9, 4)]
void CSMain(uint invocation_id : SV_GroupIndex, uint3 group_id : SV_GroupID, [vk_push_constant] uniform PushConstants pc) {
    uint thread_count = pc.create_info.workgroup_count_tile_size_px.x * pc.create_info.workgroup_count_tile_size_px.y * pc.create_info.workgroup_count_tile_size_px.z;
    uint light_count = pc.light_count; // TODO: Get this from the light buffer, dynamic range binding in the render graph
    uint num_batches = (light_count + thread_count - 1) / thread_count;
    uint tile_index = invocation_id + pc.create_info.workgroup_count_tile_size_px.x * pc.create_info.workgroup_count_tile_size_px.y * pc.create_info.workgroup_count_tile_size_px.z * group_id.z;

    uint visible_light_count = 0;
    uint visible_light_indices[128];

    for (uint batch = 0; batch < num_batches && visible_light_count < 128; ++batch) {
        uint light_index = batch * thread_count + invocation_id;
        light_index = min(light_index, light_count);

        // Build out the shared light array
        shared_lights[invocation_id] = lights[light_index];
        GroupMemoryBarrierWithGroupSync();

        for (uint light = 0; light < thread_count && visible_light_count < 128; ++light) {
            if (shared_lights[light].enabled) {
                if (test_light_in_tile(light, tile_index)) {
                    visible_light_indices[visible_light_count] = batch * thread_count + light;
                    visible_light_count++;
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

    light_grid[tile_index] = LightGridRange(offset, visible_light_count);
}