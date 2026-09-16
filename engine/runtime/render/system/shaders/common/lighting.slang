// lighting.slang

enum LightType : uint
{
    DIRECTIONAL,
    POINT,
    SPOT
};

struct PackedU6 {
    uint4 block_0;
    uint4 block_1;

    __subscript(int index) -> uint {
        get {
            if (index < 4) {
                return block_0[index];
            } else {
                return block_1[index - 4];
            }
        }

        set {
            if (index < 4) {
                block_0[index] = newValue;
            } else {
                block_1[index - 4] = newValue;
            }
        }
    }
}

struct Light
{
    float4 color_intensity;
    float4 position_falloff;
    float4 direction;
    PackedU6 shadow_map_indices;
    LightType type;
    uint shadow_map_count;
    bool enabled;
};

struct LightPayload
{
    float4 color_intensity;
    float4 position_falloff;
    float4 direction_angle;
    uint32_t type;
    uint32_t enabled;
    uint32_t padding[2];
};

enum ShadowDebugMode : uint
{
    NONE = 0,
    CASCADES = 1,
    SHADOW_FACTOR = 2,
    CASCADE_AND_SHADOW = 3,
    SCENE_CASCADE_TINT = 4
};

struct ShadowCascadeData {
    float4x4 view_proj;
    float4 uv_offset_scale; // xy = offset in atlas, zw = scale in atlas
    float split_depth;
    float3 padding;
};

struct DirectionalShadowData {
    ShadowCascadeData cascades[4];
    uint cascade_count;
    float normal_bias;
    float depth_bias;
    uint debug_mode;
    float2 atlas_texel_size;
    float2 padding;
};

struct ShadowSampleResult {
    float factor;
    int cascade_index;
};

// Canonical 4-tap Poisson distribution
static const float2 POISSON_DISK_4[4] = {
    float2(-0.38, -0.62),
    float2( 0.65, -0.45),
    float2(-0.52,  0.55),
    float2( 0.42,  0.68)
};

[ForceInline]
ShadowSampleResult sample_csm_shadow_info(Texture2D shadow_atlas, SamplerComparisonState comparison_sampler,
                                          DirectionalShadowData* shadow_data,
                                          float3 world_pos, float3 geom_normal, float view_depth) {
    ShadowSampleResult result;
    result.factor = 1.0;
    result.cascade_index = -1;

    if (shadow_data == nullptr || shadow_data.cascade_count == 0) {
        return result;
    }

    // 1. Select Cascade
    int cascade_idx = -1;
    [unroll]
    for (uint i = 0; i < shadow_data.cascade_count; ++i) {
        if (view_depth < shadow_data.cascades[i].split_depth) {
            cascade_idx = (int)i;
            break;
        }
    }

    if (cascade_idx < 0) {
        return result; // Beyond max shadow distance
    }

    result.cascade_index = cascade_idx;
    ShadowCascadeData* cascade = &shadow_data.cascades[cascade_idx];

    // 2. Normal Offset Bias
    float3 biased_pos = world_pos + geom_normal * shadow_data.normal_bias;

    // 3. Project into Light Clip Space
    float4 light_clip = mul(cascade.view_proj, float4(biased_pos, 1.0));
    float3 light_ndc = light_clip.xyz / light_clip.w;

    // Fast vectorized frustum bounds check
    if (any(abs(light_ndc.xy) > 1.0) || light_ndc.z < 0.0 || light_ndc.z > 1.0) {
        return result;
    }

    // Convert [-1, 1] NDC to [0, 1] UV
    float2 light_uv = light_ndc.xy * 0.5 + 0.5;

    // Transform into Atlas sub-region UV
    float2 atlas_uv = cascade.uv_offset_scale.xy + light_uv * cascade.uv_offset_scale.zw;

    // Reverse-Z: lit if current_depth >= occluder depth (closer to near plane 1.0)
    float compare_depth = saturate(light_ndc.z + shadow_data.depth_bias);

    // 4. Hardware PCF (4 taps * 2x2 bilinear = 16 filtered depth samples)
    float shadow = 0.0;
    const float filter_radius = 1.5;

    [unroll]
    for (int i = 0; i < 4; ++i) {
        float2 offset_uv = atlas_uv + POISSON_DISK_4[i] * shadow_data.atlas_texel_size * filter_radius;
        shadow += shadow_atlas.SampleCmpLevelZero(comparison_sampler, offset_uv, compare_depth);
    }

    result.factor = shadow * 0.25;
    return result;
}

[ForceInline]
float sample_csm_shadow(Texture2D shadow_atlas, SamplerComparisonState comparison_sampler,
                        DirectionalShadowData* shadow_data,
                        float3 world_pos, float3 geom_normal, float view_depth) {
    return sample_csm_shadow_info(shadow_atlas, comparison_sampler, shadow_data, world_pos, geom_normal, view_depth).factor;
}

[ForceInline]
float3 get_cascade_debug_color(int cascade_index) {
    switch (cascade_index) {
        case 0: return float3(1.0, 0.25, 0.25); // Red (Cascade 0: near/detailed)
        case 1: return float3(0.25, 1.0, 0.25); // Green (Cascade 1)
        case 2: return float3(0.25, 0.5, 1.0);  // Blue (Cascade 2)
        case 3: return float3(1.0, 1.0, 0.25);  // Yellow (Cascade 3: far)
        default: return float3(0.4, 0.4, 0.4); // Outside / unshadowed
    }
}

[ForceInline]
float3 apply_shadow_debug_visualization(float3 lit_color, float shadow_factor, int cascade_index, uint debug_mode) {
    if (debug_mode == (uint)ShadowDebugMode.CASCADES) {
        return get_cascade_debug_color(cascade_index);
    } else if (debug_mode == (uint)ShadowDebugMode.SHADOW_FACTOR) {
        return float3(shadow_factor);
    } else if (debug_mode == (uint)ShadowDebugMode.CASCADE_AND_SHADOW) {
        float3 cascade_col = get_cascade_debug_color(cascade_index);
        return cascade_col * (0.25 + 0.75 * shadow_factor);
    } else if (debug_mode == (uint)ShadowDebugMode.SCENE_CASCADE_TINT) {
        float3 cascade_col = get_cascade_debug_color(cascade_index);
        float3 overlay = cascade_col * (0.25 + 0.75 * shadow_factor);
        return lerp(lit_color, overlay, 0.5);
    }
    return lit_color;
}
