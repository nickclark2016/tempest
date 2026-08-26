import "camera";
import "clusters";
import "consts";
import "lighting";
import "material";
import "mesh";
import "scene";
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
    nointerpolation uint64_t material_address : MaterialAddr;
};

struct PBROutput {
    float4 color;
    float depth;
};

float f0_to_index_of_refraction(float f0) {
    float sqrt_f0 = sqrt(clamp(f0, 0.0, 0.99));
    return (1.0 + sqrt_f0) / (1.0 - sqrt_f0);
}

VertexOutput compute_vertex(uint vertex_id, uint instance_id, uint64_t instance_buffer_address,
                            uint64_t objects_address, SceneGlobals* scene)
{
    uint* instance_buffer = (uint*)instance_buffer_address;
    ObjectData* objects = (ObjectData*)objects_address;

    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(instance.mesh, vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(scene.camera.view, world_pos);
    float4 clip_pos = mul(scene.camera.projection, view_pos);

    float3x3 model_3x3 = (float3x3)instance.model_matrix;
    float3x3 t_inv_model_3x3 = (float3x3)instance.transpose_inv_model_matrix;

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
    result.material_address = (uint64_t)instance.material;

    return result;
}

BxDFParams build_bxdf_params(const PixelData px, const MaterialState mat_state) {
    BxDFParams p = {};

    p.diffuse.weight = 1.0;
    p.diffuse.color = float3(px.diffuse.rgb);
    p.diffuse.roughness = float(px.roughness);

    p.specular.weight = 1.0;
    p.specular.color = float3(1.0);
    p.specular.roughness = float(px.roughness);
    p.specular.roughness_anisotropy = 0.0;
    half dielectric_f0 = 0.16h * half(mat_state.mat.reflectance) * half(mat_state.mat.reflectance);
    p.specular.ior = float(f0_to_index_of_refraction(dielectric_f0));

    p.metal.weight = float(px.metallic);
    p.metal.color = float3(px.diffuse.rgb);

    p.transmission.weight = float(px.transmission_factor);
    p.transmission.color = float3(px.attenuation_color);
    p.transmission.depth = float(px.attenuation_distance);
    p.transmission.scatter = float3(0.0);
    p.transmission.scatter_anisotropy = 0.0;
    p.transmission.dispersion = 0.0;
    p.transmission.abbe_number = 0.0;

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

float compute_smooth_distance_attenuation(float3 pos_to_light, float radius) {
    float distance_squared = dot(pos_to_light, pos_to_light);
    float factor = distance_squared / max(radius * radius, 1e-4);
    float smooth = saturate(1.0 - factor * factor);
    return (smooth * smooth) / max(distance_squared, 1e-4);
}

void evaluate_clustered_point_lights(
    const PixelData px,
    const BxDFParams params,
    const BxDFGeometry geometry,
    const float3 world_pos,
    const float3 view_pos,
    const float2 screen_coord,
    float3 wo,
    float n_dot_v,
    SceneGlobals* scene,
    uint64_t light_bitmask_address,
    inout half4 color)
{
    uint64_t bitmask_addr = light_bitmask_address != 0 ? light_bitmask_address : scene.light_bitmask_address;
    if (scene.light_count == 0 || bitmask_addr == 0 || scene.lights_address == 0) {
        return;
    }

    var cluster_idx_opt = get_cluster_index(view_pos, screen_coord, scene);
    if (!cluster_idx_opt.hasValue) {
        return;
    }

    uint cluster_idx = cluster_idx_opt.value;
    uint* bitmasks = (uint*)bitmask_addr;
    LightPayload* lights = (LightPayload*)scene.lights_address;
    uint words_per_cluster = scene.words_per_cluster;
    uint base_mask_idx = cluster_idx * words_per_cluster;

    let bxdf = OpenPBRBxDF();

    for (uint w = 0; w < words_per_cluster; ++w) {
        uint mask = bitmasks[base_mask_idx + w];
        uint base_light_idx = w * 32;

        while (mask != 0) {
            uint bit = firstbitlow(mask);
            uint light_idx = base_light_idx + bit;
            LightPayload light = lights[light_idx];

            float3 pos_to_light = light.position_falloff.xyz - world_pos;
            float3 wi = normalize(pos_to_light);
            float n_dot_l = saturate(dot(float3(px.shading_normal), wi));

            if (n_dot_l > 0.0) {
                let bxdf_res = bxdf.evaluate(params, geometry, wi, wo, n_dot_v, n_dot_l);
                float attenuation = compute_smooth_distance_attenuation(pos_to_light, light.position_falloff.w);
                float3 radiance = light.color_intensity.rgb * light.color_intensity.w * attenuation;
                color.rgb += half3(bxdf_res.bxdf * radiance * n_dot_l);
            }

            mask &= ~(1u << bit);
        }
    }
}

float4 evaluate_material_openpbr(
    const PixelData px,
    const MaterialState mat_state,
    SceneGlobals* scene,
    const float3 world_pos,
    const float3 view_pos,
    const float2 screen_coord,
    float shadow_factor = 1.0,
    uint64_t light_bitmask_address = 0)
{
    let bxdf = OpenPBRBxDF();
    BxDFParams params = build_bxdf_params(px, mat_state);
    BxDFGeometry geometry = build_bxdf_geometry(px);

    float3 wo = normalize(scene.camera.eye.xyz - world_pos);
    float n_dot_v = max(1e-4, dot(float3(px.shading_normal), wo));

    // Ambient light with directional hemisphere response (soft sky fill)
    float up_factor = saturate(float(px.shading_normal.y) * 0.5 + 0.5);
    float3 sky_ambient = scene.ambient_light.rgb * (0.7 + 0.3 * up_factor) * float(px.ambient_occlusion);
    half4 color = px.diffuse * half4(half3(sky_ambient), 1.0h);

    if (mat_state.mat.type == MaterialType::TRANSMISSIVE) {
        half dielectric_f0 = 0.16h * half(mat_state.mat.reflectance) * half(mat_state.mat.reflectance);
        float roughness = float(px.roughness);
        float fresnel = float(dielectric_f0) + (max(1.0 - roughness, float(dielectric_f0)) - float(dielectric_f0)) * pow(1.0 - n_dot_v, 5.0);
        float3 diffuse_ambient = px.diffuse.rgb * sky_ambient * (1.0 - params.transmission.weight);
        float3 specular_ambient = sky_ambient * fresnel * (1.0 - roughness);
        color.rgb = half3(diffuse_ambient + specular_ambient);
    }

    // Directional Sun Light
    {
        float3 wi = normalize(-scene.sun.direction.xyz);
        float n_dot_l = saturate(dot(float3(px.shading_normal), wi));

        let bxdf_result = bxdf.evaluate(params, geometry, wi, wo, n_dot_v, n_dot_l);
        float3 radiance = scene.sun.color_intensity.rgb * scene.sun.color_intensity.w;

        color.rgb += half3(bxdf_result.bxdf * (radiance * shadow_factor) * n_dot_l);
    }

    // Clustered Point Lights
    evaluate_clustered_point_lights(
        px, params, geometry,
        world_pos, view_pos, screen_coord,
        wo, n_dot_v, scene, light_bitmask_address, color);

    // Emissive
    apply_emissive(mat_state, color, px.uv);

    return color;
}
