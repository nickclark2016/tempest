import "../common/camera";
import "../common/consts";
import "../common/fsquad";
import "../common/lighting";
import "../common/material";
import "../common/scene";
import "../common/tonemap";

struct SkyboxPushConstants {
    uint64_t scene_constants_address;
    int32_t skybox_texture_index;
    int32_t sampler_index;
};

[[vk::push_constant]]
ConstantBuffer<SkyboxPushConstants> push_constants;

struct VertexOutput {
    float4 position : SV_Position;
    float3 view_direction : ViewDirection;
};

float2 direction_to_rectilinear(float3 dir) {
    float x = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float y = dir.y * 0.5 + 0.5;
    return float2(x, y);
}

[shader("vertex")]
VertexOutput VSMain(uint index_id : SV_VertexID) {
    var vertex = generate_quad_vertex(index_id);
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;
    let camera = scene.camera;

    float4 world_position = mul(camera.inv_view, mul(camera.inv_projection, vertex.position));
    float3 view_direction = world_position.xyz - camera.eye.xyz;

    var output = VertexOutput(vertex.position, view_direction);
    return output;
}

struct FragmentOutput {
    float4 color : SV_Target0;
};

[shader("fragment")]
FragmentOutput FSMain(VertexOutput vs_out) {
    float3 view_direction = normalize(vs_out.view_direction);

    float4 color = float4(0.1, 0.2, 0.35, 1.0);
    if (push_constants.skybox_texture_index >= 0) {
        float2 uv = direction_to_rectilinear(view_direction);
        color = bindless_textures[push_constants.skybox_texture_index].Sample(
            bindless_samplers[push_constants.sampler_index], uv);
    } else {
        float t = saturate(view_direction.y * 0.5 + 0.5);
        color.rgb = lerp(float3(0.6, 0.7, 0.8), float3(0.1, 0.2, 0.4), t);
    }

    var output = FragmentOutput(color);
    return output;
}
