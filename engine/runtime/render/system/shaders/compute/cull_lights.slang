import "../common/clusters";
import "../common/lighting";
import "../common/scene";

struct LightCullingPushConstants {
    uint64_t scene_constants_address;
    uint64_t clusters_address;
    uint64_t lights_address;
    uint64_t light_bitmask_address;
    uint4 cluster_counts_tile_size;
    uint32_t light_count;
    uint32_t words_per_cluster;
    uint32_t total_clusters;
    uint32_t padding;
};

[[vk::push_constant]]
ConstantBuffer<LightCullingPushConstants> push_constants;

bool sphere_intersects_aabb(float3 center, float radius, ClusterBounds bounds) {
    float3 closest = clamp(center, bounds.min_corner.xyz, bounds.max_corner.xyz);
    float3 diff = center - closest;
    return dot(diff, diff) <= (radius * radius);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    uint cluster_idx = dispatch_thread_id.x;
    if (cluster_idx >= push_constants.total_clusters) {
        return;
    }

    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    ClusterBounds* clusters = (ClusterBounds*)push_constants.clusters_address;
    LightPayload* lights = (LightPayload*)push_constants.lights_address;
    uint* bitmasks = (uint*)push_constants.light_bitmask_address;

    ClusterBounds bounds = clusters[cluster_idx];
    uint words_per_cluster = push_constants.words_per_cluster;
    uint light_count = push_constants.light_count;

    uint base_out_idx = cluster_idx * words_per_cluster;

    for (uint w = 0; w < words_per_cluster; ++w) {
        uint mask = 0;
        uint start_light = w * 32;
        uint end_light = min(start_light + 32, light_count);

        for (uint l = start_light; l < end_light; ++l) {
            LightPayload light = lights[l];
            if (light.enabled != 0) {
                float3 view_pos = mul(scene.camera.view, float4(light.position_falloff.xyz, 1.0)).xyz;
                float radius = light.position_falloff.w;
                if (sphere_intersects_aabb(view_pos, radius, bounds)) {
                    mask |= (1u << (l - start_light));
                }
            }
        }

        bitmasks[base_out_idx + w] = mask;
    }
}

