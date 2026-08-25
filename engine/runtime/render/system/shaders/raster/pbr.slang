import "../common/camera";
import "../common/consts";
import "../common/lighting";
import "../common/material";
import "../common/mesh";
import "../common/pbr_common";
import "../common/scene";
import "../common/tonemap";
import "../common/vertex_pull";
import "../common/bxdf/bxdf";
import "../common/bxdf/openpbr";

struct PBRPushConstants {
    uint64_t scene_constants_address;
    uint64_t objects_address;
    uint64_t instance_indices_address;
    uint64_t directional_shadow_address;
    int32_t linear_sampler_index;
    int32_t shadow_atlas_index;
};

[[vk::push_constant]]
ConstantBuffer<PBRPushConstants> push_constants;

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
FragmentOutput FSMain(VertexOutput input) {
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    Material* mat = (Material*)input.material_address;

    MaterialState mat_state;
    mat_state.mat = mat;
    mat_state.linear_sampler = bindless_samplers[push_constants.linear_sampler_index];

    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);

    float3 view_dir = normalize(scene.camera.eye.xyz - input.world_position);
    pd.front_face = dot(view_dir, input.normal) > 0.0;
    pd.ambient_occlusion = 1.0h;

    fetch_pixel_data(mat_state, pd);

    if (mat.type == MaterialType::MASK) {
        if (pd.diffuse.w < mat.alpha_cutoff) {
            discard;
        }
    }

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

    if (mat.type == MaterialType::TRANSMISSIVE) {
        color.a = mat.transmission_factor;
    }

    return FragmentOutput(color);
}
