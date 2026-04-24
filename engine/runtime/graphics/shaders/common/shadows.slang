// shadows.slang

struct CSMCascade {
    float4x4 light_view_projection;
    float2 atlas_offset;
    float2 atlas_scale;
    float split_depth; // Depth at which this cascade ends
    float blend_start; // Depth at which blending to next cascade starts
    float texel_size_ws; // Size of one texel in world space
    uint cascade_index;
}

struct DirectionalShadowMap {
    uint atlas_index;
    uint cascade_count;
    float2 inv_atlas_resolution;

    static const uint SHADOW_CASCADE_COUNT = 4;

    CSMCascade cascades[SHADOW_CASCADE_COUNT];
}

struct ShadowMapParameters {
    static const uint MAX_DIRECTIONAL_LIGHTS = 4;

    DirectionalShadowMap directional_shadow_maps[MAX_DIRECTIONAL_LIGHTS];
    uint directional_light_count;
}