import "camera";
import "shadows";

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
    ShadowParameters shadows;
};
