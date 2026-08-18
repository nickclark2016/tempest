import "../common/camera";
import "../common/consts";
import "../common/lighting";
import "../common/material";
import "../common/mboit";
import "../common/mesh";
import "../common/pbr_common";
import "../common/scene";
import "../common/vertex_pull";
import "../common/bxdf/bxdf";
import "../common/bxdf/openpbr";

struct OitGatherPushConstants {
    uint64_t scene_constants_address;
    uint64_t objects_address;
    uint64_t instance_indices_address;
    int32_t linear_sampler_index;
    int32_t ssao_texture_index;
    int32_t point_sampler_index;
};

[[vk::push_constant]]
ConstantBuffer<OitGatherPushConstants> push_constants;

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

    float4 color = evaluate_material_openpbr(pd, mat_state, scene, input.world_position);

    float alpha = color.a;
    if (mat.type == MaterialType::TRANSMISSIVE) {
        alpha = mat.transmission_factor;
    }

    return FragmentOutput(float4(color.rgb * alpha, alpha));
}
