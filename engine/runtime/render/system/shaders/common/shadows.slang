#ifndef TEMPEST_SHADERS_COMMON_SHADOWS_SLANG
#define TEMPEST_SHADERS_COMMON_SHADOWS_SLANG

struct CSMCascade {
    float4x4 light_view_projection;
    float2 atlas_offset;
    float2 atlas_scale;
    float split_depth;
    float blend_start;
    float texel_size_ws;
    uint cascade_index;
};

struct DirectionalShadowMap {
    uint atlas_index;
    uint cascade_count;
    float2 inv_atlas_resolution;

    static const uint SHADOW_CASCADE_COUNT = 4;
    CSMCascade cascades[SHADOW_CASCADE_COUNT];
};

struct ShadowParameters {
    static const uint MAX_DIRECTIONAL_LIGHTS = 4;
    DirectionalShadowMap directional_shadow_maps[MAX_DIRECTIONAL_LIGHTS];
    uint directional_light_count;
    uint padding[3];
};

float sample_directional_shadow(
    ShadowParameters* shadows,
    float3 world_pos,
    float view_depth,
    float n_dot_l,
    SamplerState shadow_sampler,
    uint shadow_index = 0
)
{
    if (shadows == nullptr || shadows.directional_light_count == 0)
    {
        return 1.0;
    }

    DirectionalShadowMap dir_shadows = shadows.directional_shadow_maps[shadow_index];
    uint cascade_count = dir_shadows.cascade_count;
    if (cascade_count == 0)
    {
        return 1.0;
    }

    uint cascade_idx = 0;
    for (uint i = 0; i < cascade_count; ++i)
    {
        CSMCascade cascade = dir_shadows.cascades[i];
        if (view_depth <= cascade.split_depth)
        {
            cascade_idx = i;
            break;
        }
    }

    CSMCascade cascade = dir_shadows.cascades[cascade_idx];
    float4 light_clip = mul(cascade.light_view_projection, float4(world_pos, 1.0));
    float3 ndc = light_clip.xyz / max(abs(light_clip.w), 1e-6);

    if (ndc.x < -1.0 || ndc.x > 1.0 ||
        ndc.y < -1.0 || ndc.y > 1.0 ||
        ndc.z <  0.0 || ndc.z > 1.0)
    {
        return 1.0;
    }

    float2 uv = cascade.atlas_offset + (ndc.xy * 0.5 + 0.5) * cascade.atlas_scale;
    Texture2D atlas = bindless_textures[dir_shadows.atlas_index];
    float shadow_depth = atlas.Sample(shadow_sampler, uv).r;

    float texel_bias = cascade.texel_size_ws * 0.025;
    float slope_scale = cascade.texel_size_ws * 0.5;
    float slope_bias = slope_scale * (1.0 - n_dot_l);
    float bias = max(texel_bias + slope_bias, 0.0001);

    return (ndc.z >= shadow_depth - bias) ? 1.0 : 0.0;
}

#endif // TEMPEST_SHADERS_COMMON_SHADOWS_SLANG
