#ifndef TEMPEST_SHADERS_COMMON_SHADOWS_SLANG
#define TEMPEST_SHADERS_COMMON_SHADOWS_SLANG

import "material";

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

float sample_cascade_at_ndc(
    CSMCascade cascade,
    DirectionalShadowMap dir_shadows,
    float3 ndc,
    float n_dot_l,
    SamplerState shadow_sampler,
    uint atlas_slot
)
{
    float2 half_texel = dir_shadows.inv_atlas_resolution * 0.5;
    float2 tile_min = cascade.atlas_offset + half_texel;
    float2 tile_max = cascade.atlas_offset + cascade.atlas_scale - half_texel;
    float2 base_uv = cascade.atlas_offset + (ndc.xy * 0.5 + 0.5) * cascade.atlas_scale;
    float2 uv = clamp(base_uv, tile_min, tile_max);

    Texture2D atlas = bindless_textures[atlas_slot];

    float texel_bias = cascade.texel_size_ws * 0.005;
    float slope_scale = cascade.texel_size_ws * 0.05;
    float slope_bias = slope_scale * (1.0 - n_dot_l);
    float bias = max(texel_bias + slope_bias, 0.0005);

    // 2x2 PCF kernel
    float2 texel_step = dir_shadows.inv_atlas_resolution;
    float d0 = atlas.Sample(shadow_sampler, clamp(uv + float2(-0.5, -0.5) * texel_step, tile_min, tile_max)).r;
    float d1 = atlas.Sample(shadow_sampler, clamp(uv + float2( 0.5, -0.5) * texel_step, tile_min, tile_max)).r;
    float d2 = atlas.Sample(shadow_sampler, clamp(uv + float2(-0.5,  0.5) * texel_step, tile_min, tile_max)).r;
    float d3 = atlas.Sample(shadow_sampler, clamp(uv + float2( 0.5,  0.5) * texel_step, tile_min, tile_max)).r;

    float shadow = 0.0;
    shadow += (ndc.z >= d0 - bias) ? 1.0 : 0.0;
    shadow += (ndc.z >= d1 - bias) ? 1.0 : 0.0;
    shadow += (ndc.z >= d2 - bias) ? 1.0 : 0.0;
    shadow += (ndc.z >= d3 - bias) ? 1.0 : 0.0;

    return shadow * 0.25;
}

float sample_directional_shadow(
    ShadowParameters* shadows,
    float3 world_pos,
    float view_depth,
    float n_dot_l,
    SamplerState shadow_sampler,
    int32_t shadow_atlas_override_index = -1,
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

    uint atlas_slot = (shadow_atlas_override_index >= 0) ? (uint)shadow_atlas_override_index : dir_shadows.atlas_index;

    // Find the first cascade whose bounding frustum contains this world position
    uint cascade_idx = cascade_count;
    float3 cascade_ndc = float3(0.0);

    // Initial cascade hint from view depth
    uint start_cascade = 0;
    for (uint i = 0; i < cascade_count; ++i)
    {
        if (view_depth <= dir_shadows.cascades[i].split_depth)
        {
            start_cascade = i;
            break;
        }
    }

    for (uint i = start_cascade; i < cascade_count; ++i)
    {
        CSMCascade c = dir_shadows.cascades[i];
        float4 light_clip = mul(c.light_view_projection, float4(world_pos, 1.0));
        float3 ndc = light_clip.xyz / max(abs(light_clip.w), 1e-6);

        if (abs(ndc.x) <= 1.0 && abs(ndc.y) <= 1.0 && ndc.z >= 0.0 && ndc.z <= 1.0)
        {
            cascade_idx = i;
            cascade_ndc = ndc;
            break;
        }
    }

    // Fallback: check preceding cascades if not found in higher cascades
    if (cascade_idx >= cascade_count)
    {
        for (uint i = 0; i < start_cascade; ++i)
        {
            CSMCascade c = dir_shadows.cascades[i];
            float4 light_clip = mul(c.light_view_projection, float4(world_pos, 1.0));
            float3 ndc = light_clip.xyz / max(abs(light_clip.w), 1e-6);

            if (abs(ndc.x) <= 1.0 && abs(ndc.y) <= 1.0 && ndc.z >= 0.0 && ndc.z <= 1.0)
            {
                cascade_idx = i;
                cascade_ndc = ndc;
                break;
            }
        }
    }

    // If completely outside all cascades, geometry is outside shadow range
    if (cascade_idx >= cascade_count)
    {
        return 1.0;
    }

    CSMCascade cascade = dir_shadows.cascades[cascade_idx];
    float shadow = sample_cascade_at_ndc(cascade, dir_shadows, cascade_ndc, n_dot_l, shadow_sampler, atlas_slot);

    // Smooth cascade blending in overlap region
    if (cascade_idx + 1 < cascade_count && view_depth > cascade.blend_start)
    {
        float blend_range = cascade.split_depth - cascade.blend_start;
        if (blend_range > 1e-4)
        {
            float alpha = saturate((view_depth - cascade.blend_start) / blend_range);
            CSMCascade next_c = dir_shadows.cascades[cascade_idx + 1];
            float4 next_clip = mul(next_c.light_view_projection, float4(world_pos, 1.0));
            float3 next_ndc = next_clip.xyz / max(abs(next_clip.w), 1e-6);
            if (abs(next_ndc.x) <= 1.0 && abs(next_ndc.y) <= 1.0 && next_ndc.z >= 0.0 && next_ndc.z <= 1.0)
            {
                float next_shadow = sample_cascade_at_ndc(next_c, dir_shadows, next_ndc, n_dot_l, shadow_sampler, atlas_slot);
                shadow = lerp(shadow, next_shadow, alpha);
            }
        }
    }

    return shadow;
}

#endif // TEMPEST_SHADERS_COMMON_SHADOWS_SLANG
