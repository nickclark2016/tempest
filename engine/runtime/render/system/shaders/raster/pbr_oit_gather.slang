import "../common/camera";
import "../common/lighting";
import "../common/material";
import "../common/mboit";
import "../common/mesh";
import "../common/pbr_common";
import "../common/scene";
import "../common/vertex_pull";

struct OitGatherPushConstants {
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
ConstantBuffer<OitGatherPushConstants> push_constants;

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID,
                    uint start_instance : SV_StartInstanceLocation) {
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    return compute_vertex(vertex_id, instance_id + start_instance, push_constants.instance_indices_address,
                          push_constants.objects_address, scene);
}

[shader("fragment")]
[earlydepthstencil]
[require(spvFragmentShaderPixelInterlockEXT)]
void FSMain(VertexOutput input) {
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
    float tau;
    if (mat.type == MaterialType::TRANSMISSIVE) {
        float transmission = float(pd.transmission_factor);
        float alpha_effective = alpha * (1.0 - transmission);
        tau = saturate(1.0 - alpha_effective);
    } else {
        tau = saturate(1.0 - alpha);
    }

    if (tau >= 0.999999) {
        discard;
    }

    int2 pixel_coords = int2(input.position.xy);
    float depth = input.position.z;

    beginInvocationInterlock();

    float b0 = 0.0;
    if (push_constants.zeroth_moment_storage_index >= 0) {
        b0 = bindless_storage_float[push_constants.zeroth_moment_storage_index][pixel_coords];
    }

    float4 even = float4(0.0);
    float4 odd = float4(0.0);
    if (push_constants.moments_storage_index >= 0) {
        even = bindless_storage_float4_array[push_constants.moments_storage_index][uint3(pixel_coords, 0)];
        odd = bindless_storage_float4_array[push_constants.moments_storage_index][uint3(pixel_coords, 1)];
    }

    generate_power_moments(b0, even, odd, depth, tau);

    if (push_constants.zeroth_moment_storage_index >= 0) {
        bindless_storage_float[push_constants.zeroth_moment_storage_index][pixel_coords] = b0;
    }

    if (push_constants.moments_storage_index >= 0) {
        bindless_storage_float4_array[push_constants.moments_storage_index][uint3(pixel_coords, 0)] = even;
        bindless_storage_float4_array[push_constants.moments_storage_index][uint3(pixel_coords, 1)] = odd;
    }

    endInvocationInterlock();
}
