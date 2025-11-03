// material.slang

import camera;
import clusters;
import consts;
import lighting;

enum class MaterialType : uint {
    OPAQUE,
    MASK,
    BLEND,
    TRANSMISSIVE,
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

    int16_t base_texture_id;
    int16_t normal_map_id;
    int16_t metallic_roughness_map_id;
    int16_t emissive_texture_id;
    int16_t occlusion_texture_id;
    int16_t transmission_texture_id;
    int16_t thickness_texture_id;

    MaterialType type;
};

struct MaterialState {
    Material mat;
    SamplerState linear_sampler;
};

struct LightingState {
    Light sun;
    float3 ambient_color_factor;
    uint num_point_and_spot_lights;
    StructuredBuffer<Light> point_and_spot_lights;
    Texture2D shadow_map_mt;
    StructuredBuffer<ShadowMapParameter> shadow_map_params;
    SamplerState shadow_map_sampler;
    uint3 light_grid_counts; // (num_clusters_x, num_clusters_y, num_clusters_z) for clustered lighting
    float2 light_grid_depth_bounds; // (min_depth, max_depth) for clustered lighting
    StructuredBuffer<LightGridRange> light_grid;
    StructuredBuffer<uint> global_light_list_indices;
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

static const float4x4 shadow_bias_matrix = float4x4(
    0.5, 0.0, 0.0, 0.5,
    0.0, 0.5, 0.0, 0.5,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
);

// Octohedral encoding

half2 octohedral_wrap(half2 v) {
    return (1.0h - abs(v.yx)) * select(v.xy >= 0.0h, 1.0h, -1.0h);
}

half2 encode_normal(float3 normal) {
    half3 n = (half3) normal;
    normal /= (abs(normal.x) + abs(normal.y) + abs(normal.z));
    n.xy = n.z >= 0.0h ? n.xy : octohedral_wrap(n.xy);
    n.xy = n.xy * 0.5h + 0.5h;
    return n.xy;
}

float3 decode_normal(float2 encoded_normal) {
    float2 f = encoded_normal * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += select(n.xy >= 0.0, -t, t);
    return normalize(n);
}

half3 compute_f0(const half3 base, half metallic, half reflectance) {
    half dieletric_f0 = 0.16h * reflectance * reflectance;
    half3 metallic_f0 = base;
    return lerp(half3(dieletric_f0), metallic_f0, metallic);
}

half3 compute_f0(const half3 base, half metallic, half3 reflectance) {
    half3 dielectric_f0 = 0.16h * reflectance * reflectance;
    half3 metallic_f0 = base;
    return lerp(dielectric_f0, metallic_f0, metallic);
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

half d_ggx(half roughness, half n_dot_h, half3 h, half3 n) {
    // half one_minus_noh2 = 1.0h - n_dot_h * n_dot_h;
    
    half3 nxh = cross(n, h);
    half one_minus_noh2 = dot(nxh, nxh);
    
    half alpha = n_dot_h * roughness;
    half k = roughness / (one_minus_noh2 + alpha * alpha);
    half d = k * k * half(1.0 / PI);
    return min(d, 65504.0h);

    // half3 n_cross_h = cross(n, h);
    // half a = n_dot_h * roughness;
    // half k = roughness / (dot(n_cross_h, n_cross_h) + a * a);
    // half d = k * k * (1.0h / half(PI));
    // return min(d, 65504.0h);
}

half smith_ggx(half roughness, half n_dot_v, half n_dot_l) {
    half a2 = roughness * roughness;
    half l_v = n_dot_l * sqrt((n_dot_v - a2 * n_dot_v) * n_dot_v + a2);
    half l_l = n_dot_v * sqrt((n_dot_l - a2 * n_dot_l) * n_dot_l + a2);
    half v = 0.5h / (l_v + l_l);
    return v;
}

half pow5(half x) {
    return x * x * x * x * x;
}

half3 fresnel_schlick(const half3 f0, half v_dot_h) {
    half f = pow5(1.0h - v_dot_h);
    return f0 + (1.0h - f0) * f;
}

half fresnel_schlick(half f0, half f90, half v_dot_h) {
    return f0 + (f90 - f0) * pow5(1.0h - v_dot_h);
}

half3 fresnel_schlick(const half3 f0, half f90, half v_dot_h) {
    return f0 + (f90 - f0) * pow5(1.0h - v_dot_h);
}

half3 isotropic_lobe(const PixelData px, const Light light, const half3 h, const half n_dot_v, const half n_dot_l, const half n_dot_h, const half l_dot_h) {
    half d = d_ggx(px.roughness, n_dot_h, h, px.shading_normal);
    half v = smith_ggx(px.roughness, n_dot_v, n_dot_l);
    half3 f = fresnel_schlick(px.f0, 1.0h, l_dot_h); // 1.0h is a placeholder for materials without specular reflection

    return (d * v * f);
}

half3 specular_lobe_brdf(const PixelData px, const Light light, const half3 h, const half n_dot_v, const half n_dot_l, const half n_dot_h, const half l_dot_h) {
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

half3 diffuse_lobe_brdf(const PixelData px, half n_dot_v, half n_dot_l, half l_dot_h) {
    return px.diffuse.rgb * diffuse(px.roughness, n_dot_v, n_dot_l, l_dot_h);
}

// Determine how to respect the volume and attenuation distance
half3 specular_lobe_btdf(const PixelData px, const half3 h, const half n_dot_v, const half n_dot_l, const half n_dot_h, const half l_dot_h) {
    half d = d_ggx(px.roughness, n_dot_h, h, px.shading_normal);
    half v = smith_ggx(px.roughness, n_dot_v, n_dot_l);
    half3 f = fresnel_schlick(px.f0, l_dot_h);
    half3 btdf = (d * v * f) / (4 * abs(n_dot_v) * abs(n_dot_l) + 0.0001h);
    btdf *= px.transmission_factor;
    return btdf;
}

half3 fresnel_mix(half3 layer, half3 base, half ior, half v_dot_h) {
    half sqrtf0 = (1 - ior) / (1 + ior);
    half f0 = sqrtf0 * sqrtf0;
    half fr = f0 + (1 - f0) * pow5(1 - abs(v_dot_h));
    return lerp(base, layer, fr);
}

half3 compute_metallic(const PixelData px, half3 specular, half v_dot_h) {
    return specular * fresnel_schlick(px.f0, v_dot_h);
}

half3 apply_volume_attenuation(half3 radiance, half thickness, half3 attenuation_color, half attenuation_distance) {
    if (attenuation_distance == 0) {
        return radiance;
    } else {
        // Beer's Law
        half3 transmittance = pow(attenuation_color, half3(thickness / attenuation_distance));
        return radiance * transmittance;
    }
}

half3 compute_base_model(const PixelData px, const Light light, half3 light_dir, const MaterialState mat_state, const Camera camera, const float3 world_pos, const float3 view_pos) {
    half3 v = half3(normalize(camera.eye.xyz - world_pos));
    half3 l = light_dir;
    half3 h = normalize(v + l);

    half n_dot_v = max(half(1e-4), dot(px.shading_normal, v));
    half n_dot_l = saturate(dot(px.shading_normal, l));
    half n_dot_h = saturate(dot(px.shading_normal, h));
    half l_dot_h = saturate(dot(l, h));
    half v_to_h = saturate(dot(v, h));

    half3 spec_brdf = specular_lobe_brdf(px, light, h, n_dot_v, n_dot_l, n_dot_h, l_dot_h);
    half3 diffuse_brdf = diffuse_lobe_brdf(px, n_dot_v, n_dot_l, l_dot_h);

    // If the material is transmissive, we need to compute the BTDF
    if (mat_state.mat.type == MaterialType::TRANSMISSIVE) {
        half3 transmitted = specular_lobe_btdf(px, h, n_dot_v, n_dot_l, n_dot_h, l_dot_h);
        transmitted = apply_volume_attenuation(transmitted, px.thickness_factor, px.attenuation_color, px.attenuation_distance);
    
        diffuse_brdf = lerp(diffuse_brdf, transmitted.rgb, px.transmission_factor);
    }

    half3 dielectric = fresnel_mix(spec_brdf, diffuse_brdf, 1.5h, v_to_h);
    half3 metallic = compute_metallic(px, spec_brdf, v_to_h);
    
    half3 color = lerp(dielectric, metallic, px.metallic);

    color.rgb *= (1.0h - px.transmission_factor);
    
    return color;
}

void evaluate_directional_light(const PixelData px, const MaterialState mat_state, const Camera camera, const float3 world_pos, const float3 view_pos, inout half4 color, const LightingState lights) {
    const Light lighting = lights.sun;
    half3 l = half3(-lighting.direction.xyz);   
    half3 base = compute_base_model(px, lighting, l, mat_state, camera, world_pos, view_pos);

    half visibility = 1.0h;

    // If the normal is facing towards the light, compute shadows
    half n_dot_l = saturate(dot(px.shading_normal, l));

    if (n_dot_l > 0.0h) {
        // Get the pixel position in light space
        uint cascade_count = lighting.shadow_map_count;
        uint cascade_index = 0;
        for (uint i = 0; i < cascade_count - 1; ++i) {
            uint shadow_map_index = lighting.shadow_map_indices[i];
            const ShadowMapParameter shadow_map_param = lights.shadow_map_params[shadow_map_index];

            if (view_pos.z > -shadow_map_param.cascade_split) {
                cascade_index = i + 1;
            }
        }

        uint selected_shadow_map_index = lighting.shadow_map_indices[cascade_index];
        const ShadowMapParameter shadow_map_param = lights.shadow_map_params[selected_shadow_map_index];

        // compute the xy position in the shadow map relative to the cascade bounds
        float4 shadow_coord = mul(shadow_bias_matrix, mul(shadow_map_param.view_proj_matrix, float4(world_pos, 1.0)));
        shadow_coord.xyz /= shadow_coord.w;

        // Build a matrix to transform the shadow coordinate to the space of the shadow map param region
        // lerp x from shadow_map_region.x to shadow_map_region.x + shadow_map_region.z
        // lerp y from shadow_map_region.y to shadow_map_region.y + shadow_map_region.w
        // Matrix is:
        // [shadow_map_region.z, 0, shadow_map_region.x, 0]
        // [0, shadow_map_region.w, shadow_map_region.y, 0]
        // [0, 0, 1, 0]
        // [0, 0, 0, 1]
        
        float4x4 shadow_map_transform = float4x4(
            shadow_map_param.shadow_map_region.z, 0, 0, shadow_map_param.shadow_map_region.x,
            0, shadow_map_param.shadow_map_region.w, 0, shadow_map_param.shadow_map_region.y,
            0, 0, 1, 0,
            0, 0, 0, 1
        );
        
        float4 shadow_coord_transformed = mul(shadow_map_transform, shadow_coord);
        float shadow_x = shadow_coord_transformed.x;
        float shadow_y = shadow_coord_transformed.y;

        // read the shadow map
        if (shadow_coord.z > -1.0 && shadow_coord.z < 1.0) {
            float dist = lights.shadow_map_mt.Sample(lights.shadow_map_sampler, float2(shadow_x, shadow_y)).r;
            if (shadow_coord.w > 0 && dist - 0.0005 > shadow_coord.z) {
                visibility = 0.0h;
            }
        }
    }

    half3 color_comp = (base.rgb * half3(lighting.color_intensity.rgb)) * (half(lighting.color_intensity.w) * n_dot_l * visibility);
    color.rgb += color_comp; 
}

float compute_square_falloff_attenuation(float3 pos_to_light, float radius) {
    float distance_squared = dot(pos_to_light, pos_to_light);
    float factor = distance_squared / (radius * radius);
    float smooth = max(1.0 - factor * factor, 0.0);
    return (smooth * smooth) / max(distance_squared, 1e-4);
}

uint compute_cluster_z_index(float view_z, float z_near, float z_far, uint z_slice_count) {
    view_z = min(view_z, -z_near);
    float z_far_over_near = z_far / z_near;

    float log_factor = log(-view_z / z_near) / log(z_far_over_near);
    uint z_index = uint(log_factor * z_slice_count);

    return min(z_index, z_slice_count - 1);
}

Optional<uint3> get_tile_index_coords(float3 view_pos, const float4 proj_pos, float min_z, float max_z, uint3 grid_counts) {
    if (view_pos.z > -min_z || view_pos.z < -max_z) {
        // Outside the bounds of the light grid, return no tile index
        return none;
    }

    float2 uv = (proj_pos.xy / proj_pos.w) * 0.5 + 0.5; // Normalize to [0, 1]

    // This should really never happen
    if (any(uv < 0.0h) || any(uv > 1.0h)) {
        return none;
    }

    uint tile_x = min(uint(uv.x * grid_counts.x), grid_counts.x - 1);
    uint tile_y = min(uint(uv.y * grid_counts.y), grid_counts.y - 1);
    uint tile_z = compute_cluster_z_index(view_pos.z, min_z, max_z, grid_counts.z);

    return uint3(tile_x, tile_y, tile_z);
}

void evaluate_point_lights(const PixelData px, const MaterialState mat_state, const Camera camera, const float3 world_pos, const float3 view_pos, const float4 proj_pos, inout half4 color, const LightingState lighting) {
    // Determine if the pixel is in a light grid cell
    var tile_index_coords_opt = get_tile_index_coords(view_pos, proj_pos, lighting.light_grid_depth_bounds.x, lighting.light_grid_depth_bounds.y, lighting.light_grid_counts);

    if (let tile_index_coords = tile_index_coords_opt) {
        uint tile_index = tile_index_coords.x +
                            lighting.light_grid_counts.x * tile_index_coords.y +
                            lighting.light_grid_counts.x * lighting.light_grid_counts.y * tile_index_coords.z;

        var light_grid_range = lighting.light_grid[tile_index];
        
        var end_range = light_grid_range.offset + light_grid_range.count;
        for (uint light_index = light_grid_range.offset; light_index < end_range; ++light_index) {
            uint global_light_index = lighting.global_light_list_indices[light_index];
            const var light = lighting.point_and_spot_lights[global_light_index];

            float3 pos_to_light = light.position_falloff.xyz - world_pos;
            half3 light_dir = half3(normalize(pos_to_light));

            float n_dot_l = saturate(dot(px.shading_normal, light_dir));

            const float3 base = compute_base_model(px, light, light_dir, mat_state, camera, world_pos, view_pos);

            float visibility = 1.0h;

            // If the normal is facing towards the light, compute shadows
            if (n_dot_l > 0.0h) {
                // TODO: Implement shadow mapping for point lights
            }

            float attenuation = compute_square_falloff_attenuation(pos_to_light, light.position_falloff.w);

            const var color_comp = (base.rgb * float3(light.color_intensity.rgb)) * (float(light.color_intensity.w * attenuation) * n_dot_l * visibility);
            color.rgb += half3(color_comp);
        }

    }

    // for (uint i = 0; i < lighting.num_point_and_spot_lights; ++i) {
    //     const Light light = lighting.point_and_spot_lights[i];

    //     float3 pos_to_light = light.position_falloff.xyz - world_pos;
    //     half3 light_dir = half3(normalize(pos_to_light));

    //     half n_dot_l = saturate(dot(px.shading_normal, light_dir));

    //     half3 base = compute_base_model(px, light, light_dir, mat_state, camera, world_pos, view_pos);

    //     half visibility = 1.0h;

    //     // If the normal is facing towards the light, compute shadows
    //     if (n_dot_l > 0.0h) {
    //         // TODO: Implement shadow mapping for point lights
    //     }

    //     float attenuation = compute_square_falloff_attenuation(pos_to_light, light.position_falloff.w);

    //     half3 color_comp = (base.rgb * half3(light.color_intensity.rgb)) * (half(light.color_intensity.w * attenuation) * n_dot_l * visibility);
    //     color.rgb += color_comp;
    // }
}

void evaluate_lights(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos, const float3 view_pos, const float4 proj_pos, inout half4 color) {
    evaluate_directional_light(px, mat_state, camera, world_pos, view_pos, color, lighting);
    evaluate_point_lights(px, mat_state, camera, world_pos, view_pos, proj_pos, color, lighting);
}

half fetch_metallic(const MaterialState mat_state, float2 uv, Texture2D[] textures) {
    half metallic_factor = half(mat_state.mat.metallic_factor);
    if (mat_state.mat.metallic_roughness_map_id < 0) {
        return metallic_factor;
    }
    return metallic_factor * half(textures[(int) mat_state.mat.metallic_roughness_map_id].Sample(mat_state.linear_sampler, uv).b);
}

void fetch_diffuse(const MaterialState mat_state, inout PixelData pixel, Texture2D[] textures) {
    half4 texture_factor = half4(mat_state.mat.base_texture_factor);
    pixel.diffuse = texture_factor;

    half4 color = texture_factor;

    if (mat_state.mat.base_texture_id >= 0) {
        half4 texture_color = half4(textures[(int) mat_state.mat.base_texture_id].Sample(mat_state.linear_sampler, pixel.uv));
        
        if (texture_color.a < mat_state.mat.alpha_cutoff) {
            discard;
        }

        color = texture_color * texture_factor;
    }
    
    half metallic = fetch_metallic(mat_state, pixel.uv, textures);

    pixel.diffuse = color;
    pixel.metallic = metallic;
    pixel.f0 = lerp(half3(0.04h), color.rgb, metallic);
}

void fetch_roughness(const MaterialState mat_state, inout PixelData pixel, Texture2D[] textures) {
    half roughness_factor = half(mat_state.mat.roughness_factor);
    if (mat_state.mat.metallic_roughness_map_id < 0) {
        pixel.roughness = roughness_factor;
    } else {
        pixel.roughness = roughness_factor * half(textures[(int) mat_state.mat.metallic_roughness_map_id].Sample(mat_state.linear_sampler, pixel.uv).g);
    }
}

void fetch_normal(const MaterialState mat_state, inout PixelData pixel, Texture2D[] textures)
{
    if (mat_state.mat.normal_map_id < 0) {
        pixel.shading_normal = pixel.geom_normal;
    } else {
        // map rg [0, 1] -> [-1, 1]
        // map b (0.5, 1] -> [0, 1]
        half3 tan_normal = half3(textures[(int) mat_state.mat.normal_map_id].Sample(mat_state.linear_sampler, pixel.uv).rgb) * 2.0h - 1.0h;
        tan_normal.rg *= half(mat_state.mat.normal_scale);
        half3 normal = tan_normal.x * pixel.geom_tangent + tan_normal.y * pixel.geom_bitangent + tan_normal.z * pixel.geom_normal;
        pixel.shading_normal = normal;
    }

    if (!pixel.front_face) {
        pixel.shading_normal = -pixel.shading_normal;
    }
}

void fetch_transmission(const MaterialState mat_state, inout PixelData pixel, Texture2D[] textures) {
    float transmission_factor = mat_state.mat.transmission_factor;
    if (mat_state.mat.transmission_texture_id >= 0) {
        transmission_factor *= textures[(int) mat_state.mat.transmission_texture_id].Sample(mat_state.linear_sampler, pixel.uv).r;
    }
    pixel.transmission_factor = half(transmission_factor);

    float thickness_factor = mat_state.mat.thickness_factor;
    if (mat_state.mat.thickness_texture_id >= 0) {
        thickness_factor *= textures[(int) mat_state.mat.thickness_texture_id].Sample(mat_state.linear_sampler, pixel.uv).r;
    }
    pixel.thickness_factor = half(thickness_factor);

    pixel.attenuation_color = half3(mat_state.mat.attenuation_color.rgb);
    pixel.attenuation_distance = half(mat_state.mat.attenuation_distance);
}

void fetch_pixel_data(const MaterialState mat_state, inout PixelData pixel, Texture2D[] textures) {
    fetch_diffuse(mat_state, pixel, textures);
    fetch_roughness(mat_state, pixel, textures);
    fetch_normal(mat_state, pixel, textures);

    if (mat_state.mat.type == MaterialType::TRANSMISSIVE) {
        fetch_transmission(mat_state, pixel, textures);
    } else {
        pixel.transmission_factor = 0.0h;
    }
}

void apply_emissive(const MaterialState mat_state, inout half4 color, const half2 uv, Texture2D[] textures) {
    half4 emissive = half4(mat_state.mat.emissive_factor);
    
    if (mat_state.mat.emissive_texture_id >= 0) {
        Texture2D emissive_texture = textures[(int) mat_state.mat.emissive_texture_id];
        
        float4 emissive_color = emissive_texture.Sample(mat_state.linear_sampler, uv);
        emissive_color.rgb *= emissive_color.a;

        emissive.rgb *= half3(emissive_color.rgb);
    }

    color.rgb += emissive.rgb;
}

float4 evaluate_material(const PixelData px, const MaterialState mat_state, const LightingState lighting, const Camera camera, const float3 world_pos, const float3 view_pos, const float4 proj_pos, Texture2D[] textures) {
    half4 color = px.diffuse * half4(half3(lighting.ambient_color_factor.rgb) * px.ambient_occlusion, 1.0h);

    evaluate_lights(px, mat_state, lighting, camera, world_pos, view_pos, proj_pos, color);
    apply_emissive(mat_state, color, px.uv, textures);

    return color;
}
