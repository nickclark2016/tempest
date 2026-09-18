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
    uint64_t light_bitmask_address;
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
[earlydepthstencil]
FragmentOutput FSMain(VertexOutput input, bool is_front_face : SV_IsFrontFace) {
    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);
    
    MaterialState mat_state;
    mat_state.mat_address = WaveReadLaneFirst(input.material_address);
    mat_state.linear_sampler = bindless_samplers[push_constants.linear_sampler_index];
    pd.front_face = is_front_face;
    pd.ambient_occlusion = 1.0h;

    fetch_pixel_data<AlphaPolicy::Opaque>(mat_state, pd);
    
    
    float view_depth = -input.view_position.z;
    let shadow_atlas = bindless_textures[push_constants.shadow_atlas_index];
    let scene = (SceneGlobals*)WaveReadLaneFirst(push_constants.scene_constants_address);
    let shadow_comparison_sampler_idx = WaveReadLaneFirst(scene.shadow_comparison_sampler_index);
    let cmp_sampler = bindless_comparison_samplers[shadow_comparison_sampler_idx];
    let shadow_data = (DirectionalShadowData*)WaveReadLaneFirst(push_constants.directional_shadow_address);
    let shadow_res = sample_csm_shadow_info(shadow_atlas, cmp_sampler, shadow_data, input.world_position, input.normal, view_depth);
    let sun_shadow = shadow_res.factor;
    let cascade_idx = shadow_res.cascade_index;

    let wo = normalize(scene.camera.eye.xyz - input.world_position);

    float4 color = evaluate_material_openpbr(
        pd, mat_state, scene,
        input.world_position, input.view_position, input.position.xy,
        wo, sun_shadow, push_constants.light_bitmask_address);

    if (shadow_data != nullptr && shadow_data.debug_mode != 0) {
        color.rgb = apply_shadow_debug_visualization(color.rgb, sun_shadow, cascade_idx, shadow_data.debug_mode);
    }

    return FragmentOutput(color);
}
