import "camera";

struct DirectionalLight {
    float4 color_intensity; // xyz = color, w = intensity
    float4 direction;       // xyz = forward direction, w = 0
};

struct SceneGlobals {
    Camera camera;
    float4 ambient_light;
    DirectionalLight sun;
    float2 screen_size;
    float2 inv_screen_size;

    uint64_t lights_address;
    uint64_t light_bitmask_address;
    uint32_t light_count;
    uint32_t words_per_cluster;
    uint4 cluster_counts_tile_size;
    float4 cluster_depth_params;
};
