#ifndef tempest_lighting_slang
#define tempest_lighting_slang

enum LightType
{
    DIRECTIONAL,
    POINT,
    SPOT
};

struct ShadowMapParameter
{
    float4x4 inv_view_proj_matrix;
    float4 shadow_map_region; // x, y, width, height - normalized
    float cascade_split;
    
    // Padding
    float _padding0;
    float _padding1;
    float _padding2;
};

struct Light
{
    float4 color_intensity;
    float4 position;
    float4 direction;
    float4 attenuation;
    uint shadow_map_indices[6]; // Up to 6 cascades, or 1 cascade per face of a cube map
    LightType type;
    uint shadow_map_count;
};

#endif // tempest_lighting_slang