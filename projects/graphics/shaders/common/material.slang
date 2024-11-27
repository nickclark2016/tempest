#ifndef tempest_material_slang
#define tempest_material_slang

#include "camera.slang"
#include "consts.slang"
#include "lighting.slang"

enum class MaterialType : uint {
    OPAQUE,
    BLEND,
    MASK,
};

struct Material {
    float4 base_texture_factor;
    float4 emissive_factor;
    
    float normal_scale;
    float metallic_factor;
    float roughness_factor;
    float alpha_cutoff;
    float reflectance;

    int16_t base_texture_id;
    int16_t normal_map_id;
    int16_t metallic_roughness_map_id;
    int16_t emissive_texture_id;
    int16_t occlusion_texture_id;

    MaterialType type;
};

struct MaterialState {
    Material mat;
    SamplerState linear_sampler;
    Texture2D textures[BINDLESS_TEXTURE_COUNT];
};

struct LightingState {
    Light sun;
    float3 ambient_color_factor;
};

struct PixelData {
    half3 geom_tangent;
    half3 geom_bitangent;
    half3 geom_normal;
    half3 f0;
    half4 diffuse;
    half3 shading_normal;
    half2 uv;
    half roughness;
};

// Octohedral encoding

half2 octohedral_wrap(half2 v) {
    return (1.0h - abs(v.yx)) * select(v.xy >= 0.0h, 1.0h, -1.0h);
}

half2 encode_normal(half3 normal) {
    normal /= (abs(normal.x) + abs(normal.y) + abs(normal.z));
    normal.xy = normal.z >= 0.0h ? normal.xy : octohedral_wrap(normal.xy);
    normal.xy = normal.xy * 0.5h + 0.5h;
    return normal.xy;
}

float3 decode_normal(float2 encoded_normal) {
    float2 f = encoded_normal * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += select(n.xy >= 0.0, -t, t);
    return normalize(n);
}

half3 compute_diffuse(const half4 base, half metallic) {
    return base.rgb * (1.0h - metallic);
}

half3 compute_f0(const half4 base, half metallic, half reflectance) {
    return base.rgb * metallic + (reflectance * (1.0h - metallic));
}

half compute_dielectric_f0(half reflectance) {
    return 0.16h * reflectance * reflectance; 
}

half compute_metallic_from_specular(const half3 specular) {
    return max(specular.x, max(specular.y, specular.z));
}

half perceptual_roughness_to_alpha_roughness(half perceptual) {
    return perceptual * perceptual;
}

half index_of_refraction_to_f0(half transmitted, half incident) {
    half base = (transmitted - incident) / (transmitted + incident);
    return base * base;
}

half f0_to_index_of_refraction(half f0) {
    half r = sqrt(f0);
    return (1.0h + r) / (1.0h - r);
}

half ggx(half roughness, half n_dot_h, half3 h) {
    half one_minus_noh2 = 1.0h - n_dot_h * n_dot_h;
    half alpha = n_dot_h * roughness;
    half k = roughness / (one_minus_noh2 + alpha * alpha);
    half d = k * k * (1.0h / half(PI));
    return saturate(d);
}

half smith_ggx(half roughness, half n_dot_v, half n_dot_l) {
    half a2 = roughness * roughness;
    half l_v = n_dot_l * sqrt((n_dot_v - a2 * n_dot_v) * n_dot_v + a2);
    half l_l = n_dot_v * sqrt((n_dot_l - a2 * n_dot_l) * n_dot_l + a2);
    half v = 0.5h / (l_v + l_l);
    return saturate(v);
}

half3 fresnel_schlick(const half3 f0, half v_dot_h) {
    half f = pow(1.0h - v_dot_h, 5.0h);
    return f + f0 * (1.0h - f);
}

half fresnel_schlick(half f0, half f90, half v_dot_h) {
    return f0 + (f90 - f0) * pow(1.0h - v_dot_h, 5.0h);
}

half3 isotropic_lobe(const PixelData px, const Light light, const half3 h, const half n_dot_v, const half n_dot_l, const half n_dot_h, const half l_dot_h) {
    half d = ggx(px.roughness, n_dot_h, h);
    half v = smith_ggx(px.roughness, n_dot_v, n_dot_l);
    half3 f = fresnel_schlick(px.f0, l_dot_h);

    return (d * v) * f;
}

half3 specular_lobe(const PixelData px, const Light light, const half3 h, const half n_dot_v, const half n_dot_l, const half n_dot_h, const half l_dot_h) {
    return isotropic_lobe(px, light, h, n_dot_v, n_dot_l, n_dot_h, l_dot_h);
}

half diffuse_lambert() {
    return 1.0h / half(PI);
}

half diffuse_burley(half roughness, half n_dot_v, half n_dot_l, half l_dot_h) {
    half f90 = 0.5h + 2.0h * roughness * l_dot_h * l_dot_h;
    half view_scatter = fresnel_schlick(1.0h, f90, n_dot_v);
    half light_scatter = fresnel_schlick(1.0h, f90, n_dot_l);
    return light_scatter * view_scatter * diffuse_lambert();
}

half diffuse(half roughness, half n_dot_v, half n_dot_l, half l_dot_h) {
    return diffuse_burley(roughness, n_dot_v, n_dot_l, l_dot_h);
}

half3 diffuse_lobe(const PixelData px, half n_dot_v, half n_dot_l, half l_dot_h) {
    return px.diffuse.rgb * diffuse(px.roughness, n_dot_v, n_dot_l, l_dot_h);
}

void evaluate_directional_light(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos, inout half3 color) {
    half3 v = half3(normalize(camera.eye.xyz - world_pos));
    half3 l = half3(-lighting.sun.direction.xyz);
    half3 h = normalize(v + l);
    half n_dot_v = saturate(dot(px.shading_normal, v));
    half n_dot_l = saturate(dot(px.shading_normal, l));
    half n_dot_h = saturate(dot(px.shading_normal, h));
    half l_dot_h = saturate(dot(l, h));

    half3 fr = specular_lobe(px, lighting.sun, h, n_dot_v, n_dot_l, n_dot_h, l_dot_h);
    half3 fd = diffuse_lobe(px, n_dot_v, n_dot_l, l_dot_h);

    half3 dr = fd + fr;

    half3 base = dr;
    color += (base.rgb * half3(lighting.sun.color_intensity.rgb)) * (half(lighting.sun.color_intensity.w) * n_dot_l); 
}

void evaluate_lights(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos, inout half3 color) {
    evaluate_directional_light(px, mat_state, lighting, camera, world_pos, color);
}

half fetch_metallic(const MaterialState mat_state, float2 uv) {
    if (mat_state.mat.metallic_roughness_map_id < 0) {
        return 0.0h;
    }
    return half(mat_state.textures[(int) mat_state.mat.metallic_roughness_map_id].Sample(mat_state.linear_sampler, uv).b);
}

half3 compute_diffuse(const half3 base, half metallic) {
    return base * (1.0h - metallic);
}

void fetch_diffuse(const MaterialState mat_state, inout PixelData pixel) {
    half4 texture_factor = half4(mat_state.mat.base_texture_factor);
    if (mat_state.mat.base_texture_id < 0) {
        pixel.diffuse = texture_factor;
        return;
    }

    half4 texture_color = half4(mat_state.textures[(int) mat_state.mat.base_texture_id].Sample(mat_state.linear_sampler, pixel.uv));

    if (texture_color.a < mat_state.mat.alpha_cutoff) {
        discard;
    }

    half4 color = texture_color * texture_factor;
    
    half metallic = fetch_metallic(mat_state, pixel.uv);
    half reflectance = compute_dielectric_f0(half(mat_state.mat.reflectance));

    pixel.diffuse.rgb = compute_diffuse(color.rgb, metallic);
    pixel.diffuse.a = color.a;
    pixel.f0 = compute_f0(color, metallic, reflectance);
}

void fetch_roughness(const MaterialState mat_state, inout PixelData pixel) {
    if (mat_state.mat.metallic_roughness_map_id < 0) {
        pixel.roughness = 0.0h;
    } else {
        pixel.roughness = half(mat_state.textures[(int) mat_state.mat.metallic_roughness_map_id].Sample(mat_state.linear_sampler, pixel.uv).g);
    }
}

void fetch_normal(const MaterialState mat_state, inout PixelData pixel)
{
    if (mat_state.mat.normal_map_id < 0)
    {
        pixel.shading_normal = pixel.geom_normal;
        return;
    }

    // map rg [0, 1] -> [-1, 1]
    // map b (0.5, 1] -> [0, 1]
    half3 tan_normal = half3(mat_state.textures[(int) mat_state.mat.normal_map_id].Sample(mat_state.linear_sampler, pixel.uv).rgb) * 2.0h - 1.0h;

    tan_normal.rg *= half(mat_state.mat.normal_scale);

    half3 normal = tan_normal.x * pixel.geom_tangent + tan_normal.y * pixel.geom_bitangent + tan_normal.z * pixel.geom_normal;
    pixel.shading_normal = normal;
}

void apply_emissive(const MaterialState mat_state, inout half4 color, const half2 uv) {
    half4 emissive = half4(mat_state.mat.emissive_factor);
    
    if (mat_state.mat.emissive_texture_id >= 0) {
        emissive *= half4(mat_state.textures[(int) mat_state.mat.emissive_texture_id].Sample(mat_state.linear_sampler, uv));
    }

    half attenuation = lerp(1.0h, 5.0h, emissive.w);
    attenuation *= color.a;

    color.rgb += emissive.rgb * attenuation;
}

void fetch_pixel_data(const MaterialState mat_state, inout PixelData pixel) {
    fetch_diffuse(mat_state, pixel);
    fetch_roughness(mat_state, pixel);
    fetch_normal(mat_state, pixel);
}

float4 evaluate_material(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos) {
    half4 color = px.diffuse * half4(half3(lighting.ambient_color_factor.rgb), 1.0h);

    evaluate_lights(px, mat_state, lighting, camera, world_pos, color.rgb);

    // premultiply alpha
    color.rgb *= px.diffuse.a;

    apply_emissive(mat_state, color, px.uv);

    return color;
}

#endif // tempest_material_slang