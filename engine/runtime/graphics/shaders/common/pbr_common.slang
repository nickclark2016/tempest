// pbr_common.slang

import "camera";
import "clusters";
import "consts";
import "lighting";
import "material";
import "mesh";
import "scene";
import "shadows";
import "tonemap";
import "vertex_pull";
import "bxdf/bxdf";
import "bxdf/openpbr";

struct VertexOutput {
    float4 position         : SV_Position;
    float3 world_position   : Position;
    float3 view_position    : ViewPosition;
    float4 current_position : CurrentPosition;
    float2 uv               : UV0;
    float3 normal           : Normal;
    float3 tangent          : Tangent;
    float3 bitangent        : Bitangent;
    uint material_id        : Material;
};

struct FragmentOutput {
    float4 color : SV_Target0;
};

struct PBROutput {
    float4 color;
    float2 velocity;
    float depth;
};

// Geometry and scene data
[vk_binding(0, 0)] ConstantBuffer<SceneGlobals, Std430DataLayout> scene;
[vk_binding(1, 0)] StructuredBuffer<uint> vertex_pull_buffer;
[vk_binding(2, 0)] StructuredBuffer<Mesh> meshes;
[vk_binding(3, 0)] StructuredBuffer<ObjectData> objects;
[vk_binding(4, 0)] StructuredBuffer<uint> instance_buffer;
[vk_binding(5, 0)] StructuredBuffer<Material> materials;
[vk_binding(15, 0)] SamplerState linear_sampler;
[vk_binding(16, 0)] Texture2D bindless_textures[];

// Lights
[vk_binding(0, 1)] StructuredBuffer<Light> lights;
[vk_binding(1, 1)] StructuredBuffer<LightGridRange> light_grid;
[vk_binding(2, 1)] StructuredBuffer<uint> global_light_index_list;

// Shadows
[vk_binding(3, 1)] RWStructuredBuffer<ShadowMapParameters> shadow_map_params;
[vk_binding(4, 1)] SamplerState shadow_map_sampler;
[vk_binding(5, 1)] Texture2D shadow_maps[ShadowMapParameters.MAX_DIRECTIONAL_LIGHTS];

half2 calculate_velocity(half4 new_position, half4 previous_position) {
    previous_position /= previous_position.w;
    previous_position.xy = (previous_position.xy + 1.0h) * 0.5h;
    previous_position.y = 1.0h - previous_position.y;

    new_position /= new_position.w;
    new_position.xy = (new_position.xy + 1.0h) * 0.5h;
    new_position.y = 1.0h - new_position.y;

    return (new_position - previous_position).xy;
}

VertexOutput compute_vertex(uint vertex_id, uint instance_id) {
    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(scene.camera.view, world_pos);
    float4 clip_pos = mul(scene.camera.projection, view_pos);

    float3x3 model_3x3 = (float3x3) instance.model_matrix;
    float3x3 t_inv_model_3x3 = (float3x3) instance.transpose_inv_model_matrix;

    float3 geom_normal = normalize(mul(t_inv_model_3x3, v.normal));
    float3 geom_tangent = normalize(mul(model_3x3, v.tangent.xyz));
    float3 geom_bitan = normalize(cross(geom_normal, geom_tangent)) * v.tangent.w;

    VertexOutput result;

    result.position = clip_pos;
    result.current_position = clip_pos;
    result.world_position = world_pos.xyz;
    result.view_position = view_pos.xyz;
    result.uv = v.uv0;
    result.normal = geom_normal;
    result.tangent = geom_tangent;
    result.bitangent = geom_bitan;
    result.material_id = instance.material_id;

    return result;
}

BxDFParams build_bxdf_params(const PixelData px, const MaterialState mat_state) {
    BxDFParams p = {};

    // Diffuse
    p.diffuse.weight = 1.0;
    p.diffuse.color = float3(px.diffuse.rgb);
    p.diffuse.roughness = float(px.roughness);

    // Specular
    p.specular.weight = 1.0;
    p.specular.color = float3(1.0);
    p.specular.roughness = float(px.roughness);
    p.specular.roughness_anisotropy = 0.0;
    // Derive IOR from the dielectric reflectance (f0 = 0.16 * reflectance^2)
    half dielectric_f0 = 0.16h * half(mat_state.mat.reflectance) * half(mat_state.mat.reflectance);
    p.specular.ior = float(f0_to_index_of_refraction(dielectric_f0));

    // Metal
    p.metal.weight = float(px.metallic);
    p.metal.color = float3(px.diffuse.rgb);

    // Transmission
    p.transmission.weight = float(px.transmission_factor);
    p.transmission.color = float3(px.attenuation_color);
    p.transmission.depth = float(px.attenuation_distance);
    p.transmission.scatter = float3(0.0);
    p.transmission.scatter_anisotropy = 0.0;
    p.transmission.dispersion = 0.0;
    p.transmission.abbe_number = 0.0;

    // Subsurface, thin film, coat, fuzz: default to zero

    // Geometry
    p.geometry.opacity = float(px.diffuse.a);
    p.geometry.thin_walled = false;

    return p;
}

BxDFGeometry build_bxdf_geometry(const PixelData px) {
    BxDFGeometry g;
    g.normal = float3(px.shading_normal);
    g.geometric_normal = float3(px.geom_normal);
    g.tangent = float3(px.geom_tangent);
    g.bitangent = float3(px.geom_bitangent);
    g.coat_normal = float3(px.shading_normal);
    g.coat_tangent = float3(px.geom_tangent);
    g.coat_bitangent = float3(px.geom_bitangent);
    return g;
}

float4 evaluate_material_openpbr(
    const PixelData px,
    const MaterialState mat_state,
    const LightingState lighting,
    const Camera camera,
    const float3 world_pos,
    const float3 view_pos,
    const float4 proj_pos,
    SamplerState shadow_sampler,
    RWStructuredBuffer<ShadowMapParameters> shadows,
    Texture2D[ShadowMapParameters.MAX_DIRECTIONAL_LIGHTS] dir_shadow_maps,
    Texture2D[] textures)
{
    let bxdf = OpenPBRBxDF();
    BxDFParams params = build_bxdf_params(px, mat_state);
    BxDFGeometry geometry = build_bxdf_geometry(px);

    float3 wo = normalize(camera.eye.xyz - world_pos);
    float n_dot_v = max(1e-4, dot(float3(px.shading_normal), wo));

    // Ambient
    half4 color = px.diffuse * half4(half3(lighting.ambient_color_factor.rgb) * px.ambient_occlusion, 1.0h);

    // Directional light
    {
        const Light sun = lighting.sun;
        float3 wi = float3(-sun.direction.xyz);
        float n_dot_l = saturate(dot(float3(px.shading_normal), wi));

        let bxdf_result = bxdf.evaluate(params, geometry, wi, wo, n_dot_v, n_dot_l);

        half visibility = 1.0h;
        if (n_dot_l > 0.0) {
            visibility = sample_directional_shadow(
                shadows, world_pos, -view_pos.z, n_dot_l,
                shadow_sampler, dir_shadow_maps
            );
        }

        float3 radiance = sun.color_intensity.rgb * sun.color_intensity.w;
        color.rgb += half3(bxdf_result.bxdf * radiance * n_dot_l * float(visibility));
    }

    // Clustered point and spot lights
    {
        var tile_opt = get_tile_index_coords(
            view_pos, proj_pos,
            lighting.light_grid_depth_bounds.x,
            lighting.light_grid_depth_bounds.y,
            lighting.light_grid_counts
        );

        if (let tile = tile_opt) {
            uint tile_index = tile.x
                + lighting.light_grid_counts.x * tile.y
                + lighting.light_grid_counts.x * lighting.light_grid_counts.y * tile.z;

            var range = lighting.light_grid[tile_index];
            uint end_range = range.offset + range.count;

            for (uint i = range.offset; i < end_range; ++i) {
                uint global_idx = lighting.global_light_list_indices[i];
                const var light = lighting.point_and_spot_lights[global_idx];

                float3 pos_to_light = light.position_falloff.xyz - world_pos;
                float3 wi = normalize(pos_to_light);
                float n_dot_l = saturate(dot(float3(px.shading_normal), wi));

                let bxdf_result = bxdf.evaluate(params, geometry, wi, wo, n_dot_v, n_dot_l);

                float attenuation = compute_square_falloff_attenuation(pos_to_light, light.position_falloff.w);
                float3 radiance = light.color_intensity.rgb * light.color_intensity.w * attenuation;

                color.rgb += half3(bxdf_result.bxdf * radiance * n_dot_l);
            }
        }
    }

    // Emissive
    apply_emissive(mat_state, color, px.uv, textures);

    return color;
}

PBROutput compute_fragment(VertexOutput input, Texture2D ao_texture) {
    LightingState lighting;
    lighting.sun = scene.sun;
    lighting.ambient_color_factor = scene.ambient_light.rgb;
    lighting.point_and_spot_lights = lights;
    lighting.num_point_and_spot_lights = scene.point_light_count;
    lighting.light_grid_counts = scene.tile_cluster_count_and_size.xyz;
    lighting.light_grid_depth_bounds = scene.light_cluster_depth_range;
    lighting.light_grid = light_grid; // Light grid for clustered lighting
    lighting.global_light_list_indices = global_light_index_list; // For clustered lighting

    MaterialState material;
    material.mat = materials[input.material_id];
    material.linear_sampler = linear_sampler;

    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);
    
    float3 view_dir = normalize(scene.camera.eye.xyz - input.world_position);
    pd.front_face = dot(view_dir, input.normal) > 0.0;

    float2 screen_uv = input.position.xy / scene.screen_size;

    fetch_pixel_data(material, pd, bindless_textures);
    half ao = (half) ao_texture.Sample(linear_sampler, screen_uv).r;
    pd.ambient_occlusion = pow(ao, (half) scene.ao_strength);

    float4 color = evaluate_material_openpbr(pd, material, lighting, scene.camera, input.world_position, input.view_position, input.current_position, shadow_map_sampler, shadow_map_params, shadow_maps, bindless_textures);

    PBROutput output;

    output.color = color;
    output.depth = compute_depth(input.current_position);
    
    if (material.mat.type == MaterialType::TRANSMISSIVE) {
        output.color.a = material.mat.transmission_factor;
    }

    return output;
}

float compute_depth(float4 clip_pos) {
    float f_ndc_depth = clip_pos.z / clip_pos.w;
    return f_ndc_depth;
}

