#ifndef tempest_lighting_slang
#define tempest_lighting_slang

struct DirectionalLight {
    float4 direction;
    float4 color_intensity;
};

struct PointLight {
    float4 location;
    float4 color;
    float range;
    float intensity;
};

#endif // tempest_lighting_slang