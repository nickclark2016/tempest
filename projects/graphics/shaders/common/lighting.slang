// lighting.slang

enum LightType : uint
{
    DIRECTIONAL,
    POINT,
    SPOT
};

struct ShadowMapParameter
{
    float4x4 view_proj_matrix;
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
    float4 attenuation; // x = constant, y = linear, z = quadratic, w = range
    Array<uint, 6> shadow_map_indices; // Up to 6 cascades, or 1 cascade per face of a cube map
    LightType type;
    uint shadow_map_count;
};
