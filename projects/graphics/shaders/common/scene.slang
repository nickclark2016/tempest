import "camera";
import "lighting";

struct SceneGlobals {
    Camera camera;
    float2 screen_size;
    float4 ambient_light;
    Light sun;
    uint4 tile_cluster_count_and_size;
    float2 light_cluster_depth_range;
    float ao_strength;
    uint point_light_count;
};