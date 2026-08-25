import "../common/camera";
import "../common/lighting";
import "../common/material";
import "../common/mboit";
import "../common/mesh";
import "../common/pbr_common";
import "../common/scene";
import "../common/vertex_pull";
import "../common/bxdf/bxdf";
import "../common/bxdf/openpbr";

struct OitResolvePushConstants {
    uint64_t scene_constants_address;
    uint64_t objects_address;
    uint64_t instance_indices_address;
    uint64_t directional_shadow_address;
    int32_t moments_storage_index;
    int32_t zeroth_moment_storage_index;
    int32_t linear_sampler_index;
    int32_t point_sampler_index;
    int32_t ssao_texture_index;
    int32_t shadow_atlas_index;
};

[[vk::push_constant]]
ConstantBuffer<OitResolvePushConstants> push_constants;

struct FragmentOutput {
    float4 color : SV_Target0;
};

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID,
                    uint start_instance : SV_StartInstanceLocation) {
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    return compute_vertex(vertex_id, instance_id + start_instance, push_constants.instance_indices_address,
                          push_constants.objects_address, scene);
}

[shader("fragment")]
[earlydepthstencil]
FragmentOutput FSMain(VertexOutput input) {
    Material* mat = (Material*)input.material_address;
    if (mat.type != MaterialType::BLEND && mat.type != MaterialType::TRANSMISSIVE) {
        discard;
    }

    MaterialState mat_state;
    mat_state.mat = mat;
    mat_state.linear_sampler = bindless_samplers[push_constants.linear_sampler_index];

    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);

    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    float3 view_dir = normalize(scene.camera.eye.xyz - input.world_position);
    pd.front_face = dot(view_dir, input.normal) > 0.0;
    pd.ambient_occlusion = 1.0h;

    fetch_pixel_data(mat_state, pd);

    float alpha = float(pd.diffuse.a);
    if (mat.type == MaterialType::TRANSMISSIVE) {
        float transmission = float(pd.transmission_factor);
        half dielectric_f0 = 0.16h * half(mat_state.mat.reflectance) * half(mat_state.mat.reflectance);
        float v_dot_n = saturate(dot(view_dir, float3(pd.shading_normal)));
        float fresnel = float(dielectric_f0) + (1.0 - float(dielectric_f0)) * pow(1.0 - v_dot_n, 5.0);
        alpha = alpha * (1.0 - transmission * (1.0 - fresnel));
    }

    int2 pixel_coords = int2(input.position.xy);
    float depth = input.position.z;

    float b0 = 0.0;
    if (push_constants.zeroth_moment_storage_index >= 0) {
        b0 = bindless_storage_float[push_constants.zeroth_moment_storage_index][pixel_coords];
    }

    if (b0 < 0.001005) {
        discard;
    }

    float4 b_even_q = float4(0.0);
    float4 b_odd_q = float4(0.0);
    if (push_constants.moments_storage_index >= 0) {
        b_even_q = bindless_storage_float4_array[push_constants.moments_storage_index][uint3(pixel_coords, 0)];
        b_odd_q = bindless_storage_float4_array[push_constants.moments_storage_index][uint3(pixel_coords, 1)];
    }

    float4 b_even, b_odd;
    offset_and_dequantize_moments(b_even, b_odd, b_even_q, b_odd_q);

    const float bias_vector[8] = {
        0.0,
        0.42474916387959866,
        0.0,
        0.22407802675585284,
        0.0,
        0.15369230769230768,
        0.0,
        0.12900440529089119,
    };

    float transmittance = compute_transmittance_at_depth(b0, b_even, b_odd, depth, 0.025, 0.1, bias_vector);

    float sun_shadow = 1.0;
    int cascade_idx = -1;
    DirectionalShadowData* shadow_data = nullptr;
    if (push_constants.shadow_atlas_index >= 0 && push_constants.directional_shadow_address != 0) {
        shadow_data = (DirectionalShadowData*)push_constants.directional_shadow_address;
        float view_depth = -input.view_position.z;
        Texture2D shadow_atlas = bindless_textures[push_constants.shadow_atlas_index];
        ShadowSampleResult shadow_res = sample_csm_shadow_info(shadow_atlas, mat_state.linear_sampler, shadow_data, input.world_position, input.normal, view_depth);
        sun_shadow = shadow_res.factor;
        cascade_idx = shadow_res.cascade_index;
    }

    float4 color = evaluate_material_openpbr(pd, mat_state, scene, input.world_position, sun_shadow);

    if (shadow_data != nullptr && shadow_data.debug_mode != 0) {
        color.rgb = apply_shadow_debug_visualization(color.rgb, sun_shadow, cascade_idx, shadow_data.debug_mode);
    }

    float4 result = float4(color.rgb * alpha * transmittance, alpha * transmittance);
    return FragmentOutput(result);
}
