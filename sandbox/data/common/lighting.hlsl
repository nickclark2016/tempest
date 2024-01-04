#ifndef tempest_common_lighting_hlsl
#define tempest_common_lighting_hlsl

struct DirectionalLight
{
    float4 direction;
    float4 color_illum;
};

struct PointLight
{
    float4 location;
    float4 color;
    float range;
    float intensity;
};

#endif // tempest_common_lighting_hlsl