import "camera";
import "colorspace";
import "consts";
import "lighting";

enum class MaterialType : uint {
    OPAQUE = 0,
    MASK = 1,
    BLEND = 2,
    TRANSMISSIVE = 3,
};

struct Material {
    float4 base_texture_factor;
    float4 emissive_factor;
    float4 attenuation_color;

    float normal_scale;
    float metallic_factor;
    float roughness_factor;
    float alpha_cutoff;
    float reflectance;
    float transmission_factor;
    float thickness_factor;
    float attenuation_distance;

    int32_t base_texture_id;
    int32_t normal_map_id;
    int32_t metallic_roughness_map_id;
    int32_t emissive_texture_id;
    int32_t occlusion_texture_id;
    int32_t transmission_texture_id;
    int32_t thickness_texture_id;

    MaterialType type;
};

struct MaterialState {
    uint64_t mat_address;
    SamplerState linear_sampler;

    property Material* mat {
        get { return (Material*)mat_address; }
        set { mat_address = (uint64_t)newValue; }
    }
};

struct PixelData {
    half3 geom_tangent;
    half3 geom_bitangent;
    half3 geom_normal;
    half3 f0;
    half4 diffuse;
    half3 shading_normal;
    half2 uv;
    half ambient_occlusion;
    half roughness;
    half metallic;
    half transmission_factor;
    half thickness_factor;
    half3 attenuation_color;
    half attenuation_distance;
    bool front_face;
};

// Bindless Descriptors (Set 0 = Samplers, Set 1 = Sampled Images)
[[vk::binding(0, 0)]] SamplerState bindless_samplers[];
[[vk::binding(0, 1)]] Texture2D bindless_textures[];

half fetch_metallic(const MaterialState mat_state, float2 uv) {
    half metallic_factor = half(mat_state.mat.metallic_factor);
    if (mat_state.mat.metallic_roughness_map_id < 0) {
        return metallic_factor;
    }
    return metallic_factor * half(bindless_textures[(int) mat_state.mat.metallic_roughness_map_id].Sample(mat_state.linear_sampler, uv).b);
}

void fetch_diffuse(const MaterialState mat_state, inout PixelData pixel) {
    half4 texture_factor = half4(mat_state.mat.base_texture_factor);
    pixel.diffuse = texture_factor;

    half4 color = texture_factor;

    if (mat_state.mat.base_texture_id >= 0) {
        half4 texture_color = half4(bindless_textures[(int) mat_state.mat.base_texture_id].Sample(mat_state.linear_sampler, pixel.uv));
        
        if (mat_state.mat.type == MaterialType::MASK && texture_color.a < mat_state.mat.alpha_cutoff) {
            discard;
        }

        texture_color.rgb = iec_srgb_to_linear(texture_color.rgb);
        color = texture_color * texture_factor;
    }
    
    half metallic = fetch_metallic(mat_state, pixel.uv);

    pixel.diffuse = color;
    pixel.metallic = metallic;
    pixel.f0 = lerp(half3(0.04h), color.rgb, metallic);
}

void fetch_roughness(const MaterialState mat_state, inout PixelData pixel) {
    half roughness_factor = half(mat_state.mat.roughness_factor);
    if (mat_state.mat.metallic_roughness_map_id < 0) {
        pixel.roughness = roughness_factor;
    } else {
        pixel.roughness = roughness_factor * half(bindless_textures[(int) mat_state.mat.metallic_roughness_map_id].Sample(mat_state.linear_sampler, pixel.uv).g);
    }
}

void fetch_normal(const MaterialState mat_state, inout PixelData pixel)
{
    if (mat_state.mat.normal_map_id < 0) {
        pixel.shading_normal = pixel.geom_normal;
    } else {
        half3 tan_normal = half3(bindless_textures[(int) mat_state.mat.normal_map_id].Sample(mat_state.linear_sampler, pixel.uv).rgb) * 2.0h - 1.0h;
        tan_normal.rg *= half(mat_state.mat.normal_scale);
        half3 normal = tan_normal.x * pixel.geom_tangent + tan_normal.y * pixel.geom_bitangent + tan_normal.z * pixel.geom_normal;
        pixel.shading_normal = normalize(normal);
    }

    if (!pixel.front_face) {
        pixel.shading_normal = -pixel.shading_normal;
    }
}

void fetch_transmission(const MaterialState mat_state, inout PixelData pixel) {
    float transmission_factor = mat_state.mat.transmission_factor;
    if (mat_state.mat.transmission_texture_id >= 0) {
        transmission_factor *= bindless_textures[(int) mat_state.mat.transmission_texture_id].Sample(mat_state.linear_sampler, pixel.uv).r;
    }
    pixel.transmission_factor = half(transmission_factor);

    float thickness_factor = mat_state.mat.thickness_factor;
    if (mat_state.mat.thickness_texture_id >= 0) {
        thickness_factor *= bindless_textures[(int) mat_state.mat.thickness_texture_id].Sample(mat_state.linear_sampler, pixel.uv).r;
    }
    pixel.thickness_factor = half(thickness_factor);

    pixel.attenuation_color = half3(mat_state.mat.attenuation_color.rgb);
    pixel.attenuation_distance = half(mat_state.mat.attenuation_distance);
}

void fetch_pixel_data(const MaterialState mat_state, inout PixelData pixel) {
    fetch_diffuse(mat_state, pixel);
    fetch_roughness(mat_state, pixel);
    fetch_normal(mat_state, pixel);

    if (mat_state.mat.type == MaterialType::TRANSMISSIVE) {
        fetch_transmission(mat_state, pixel);
    } else {
        pixel.transmission_factor = 0.0h;
    }
}

void apply_emissive(const MaterialState mat_state, inout half4 color, const half2 uv) {
    half4 emissive = half4(mat_state.mat.emissive_factor);
    
    if (mat_state.mat.emissive_texture_id >= 0) {
        Texture2D emissive_texture = bindless_textures[(int) mat_state.mat.emissive_texture_id];
        float4 emissive_color = emissive_texture.Sample(mat_state.linear_sampler, uv);
        emissive_color.rgb = iec_srgb_to_linear(emissive_color.rgb);
        emissive_color.rgb *= emissive_color.a;
        emissive.rgb *= half3(emissive_color.rgb);
    }

    color.rgb += emissive.rgb;
}
