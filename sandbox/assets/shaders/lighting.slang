#ifndef tempest_lighting_slang
#define tempest_lighting_slang

enum LightType
{
    DIRECTIONAL,
    POINT,
    SPOT
};

struct Light
{
    float4 color_intensity;
    float4 position;
    float4 direction;
    float4 attenuation;
    LightType type;
};

#endif // tempest_lighting_slang