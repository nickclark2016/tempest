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
    MaterialType type;
    uint base_texture_id;
    uint normal_map_id;
    uint metallic_map_id;
    uint roughness_map_id;
    uint ao_map_id;
    float alpha_cutoff;
    float reflectance;
    float4 base_texture_factor;
};

struct MaterialState {
    Material mat;
    SamplerState linear_sampler;
    Texture2D textures[BINDLESS_TEXTURE_COUNT];
};

struct LightingState {
    DirectionalLight sun;
    StructuredBuffer<PointLight> point_lights;
};

struct PixelData {
    float3 geom_tangent;
    float3 geom_bitangent;
    float3 geom_normal;
    float3 f0;
    float3 diffuse;
    float3 shading_normal;
    float2 uv;
    float roughness;
};

float3 compute_diffuse(const float4 base, float metallic) {
    return base.rgb * (1.0 - metallic);
}

float3 compute_f0(const float4 base, float metallic, float reflectance) {
    return base.rgb * metallic + (reflectance * (1.0 - metallic));
}

float compute_dielectric_f0(float reflectance) {
    return 0.16 * reflectance * reflectance; 
}

float compute_metallic_from_specular(const float3 specular) {
    return max(specular.x, max(specular.y, specular.z));
}

float perceptual_roughness_to_alpha_roughness(float perceptual) {
    return perceptual * perceptual;
}

float index_of_refraction_to_f0(float transmitted, float incident) {
    float base = (transmitted - incident) / (transmitted + incident);
    return base * base;
}

float f0_to_index_of_refraction(float f0) {
    float r = sqrt(f0);
    return (1 + r) / (1 - r);
}

float ggx(float roughness, float n_dot_h, float3 h) {
    float one_minus_noh2 = 1.0 - n_dot_h * n_dot_h;
    float alpha = n_dot_h * roughness;
    float k = roughness / (one_minus_noh2 + alpha * alpha);
    float d = k * k * (1.0 / PI);
    return saturate(d);
}

float smith_ggx(float roughness, float n_dot_v, float n_dot_l) {
    float a2 = roughness * roughness;
    float l_v = n_dot_l * sqrt((n_dot_v - a2 * n_dot_v) * n_dot_v + a2);
    float l_l = n_dot_v * sqrt((n_dot_l - a2 * n_dot_l) * n_dot_l + a2);
    float v = 0.5 / (l_v + l_l);
    return saturate(v);
}

float3 fresnel_schlick(const float3 f0, float v_dot_h) {
    float f = pow(1.0 - v_dot_h, 5.0);
    return f + f0 * (1.0 - f);
}

float3 isotropic_lobe(const PixelData px, const DirectionalLight light, const float3 h, const float n_dot_v, const float n_dot_l, const float n_dot_h, const float l_dot_h) {
    float d = ggx(px.roughness, n_dot_h, h);
    float v = smith_ggx(px.roughness, n_dot_v, n_dot_l);
    float3 f = fresnel_schlick(px.f0, l_dot_h);

    return (d * v) * f;
}

float3 specular_lobe(const PixelData px, const DirectionalLight light, const float3 h, const float n_dot_v, const float n_dot_l, const float n_dot_h, const float l_dot_h) {
    return isotropic_lobe(px, light, h, n_dot_v, n_dot_l, n_dot_h, l_dot_h);
}

float3 diffuse_lobe(const PixelData px, float n_dot_v, float n_dot_l, float l_dot_h) {
    return px.diffuse / PI; // TODO: Investigate other diffuse models (burley?)
}

void evaluate_directional_light(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos, inout float3 color) {
    float3 v = normalize(camera.eye.xyz - world_pos);
    float3 l = -lighting.sun.direction.xyz;
    float3 h = normalize(v + l);
    float n_dot_v = saturate(dot(px.shading_normal, v));
    float n_dot_l = saturate(dot(px.shading_normal, l));
    float n_dot_h = saturate(dot(px.shading_normal, h));
    float l_dot_h = saturate(dot(l, h));

    float3 fr = specular_lobe(px, lighting.sun, h, n_dot_v, n_dot_l, n_dot_h, l_dot_h);
    float3 fd = diffuse_lobe(px, n_dot_v, n_dot_l, l_dot_h);

    float3 dr = fd + fr;

    float3 base = dr;
    color += (base.rgb * lighting.sun.color_intensity.rgb) * (lighting.sun.color_intensity.w * n_dot_l); 
}

float fetch_metallic(const MaterialState mat_state, float2 uv) {
    if (mat_state.mat.metallic_map_id == MAX_UINT) {
        return 0.0;
    }
    return mat_state.textures[mat_state.mat.metallic_map_id].Sample(mat_state.linear_sampler, uv).b;
}

float3 compute_diffuse(const float3 base, float metallic) {
    return base * (1.0 - metallic);
}

void fetch_diffuse(const MaterialState mat_state, inout PixelData pixel) {
    float4 texture_factor = mat_state.mat.base_texture_factor;
    if (mat_state.mat.base_texture_id == MAX_UINT) {
        pixel.diffuse = texture_factor.rgb;
        return;
    }

    float4 texture_color = mat_state.textures[mat_state.mat.base_texture_id].Sample(mat_state.linear_sampler, pixel.uv);

    if (texture_color.a < mat_state.mat.alpha_cutoff) {
        discard;
    }

    float4 color = texture_color * texture_factor;
    float metallic = fetch_metallic(mat_state, pixel.uv);

    float reflectance = compute_dielectric_f0(mat_state.mat.reflectance);

    pixel.diffuse = compute_diffuse(color.rgb, metallic);
    pixel.f0 = compute_f0(color, metallic, reflectance);
}

void fetch_roughness(const MaterialState mat_state, inout PixelData pixel) {
    if (mat_state.mat.roughness_map_id == MAX_UINT) {
        pixel.roughness = 0.0;
    } else {
        pixel.roughness = mat_state.textures[mat_state.mat.metallic_map_id].Sample(mat_state.linear_sampler, pixel.uv).g;
    }
}

void fetch_normal(const MaterialState mat_state, inout PixelData pixel)
{
    if (mat_state.mat.normal_map_id == MAX_UINT)
    {
        pixel.shading_normal = pixel.geom_normal;
        return;
    }

    // map rg [0, 1] -> [-1, 1]
    // map b (0.5, 1] -> [0, 1]
    float3 tan_normal = mat_state.textures[mat_state.mat.normal_map_id].Sample(mat_state.linear_sampler, pixel.uv).rgb * 2.0 - 1.0;
    float3 normal = tan_normal.x * pixel.geom_tangent + tan_normal.y * pixel.geom_bitangent + tan_normal.z * pixel.geom_normal;
    pixel.shading_normal = normal;
}

void fetch_pixel_data(const MaterialState mat_state, inout PixelData pixel) {
    fetch_diffuse(mat_state, pixel);
    fetch_roughness(mat_state, pixel);
    fetch_normal(mat_state, pixel);
}

float4 evaluate_material(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos) {
    float3 color = px.diffuse * 0.1;

    evaluate_directional_light(px, mat_state, lighting, camera, world_pos, color);

    return float4(color, 1);
}

#endif // tempest_material_slang