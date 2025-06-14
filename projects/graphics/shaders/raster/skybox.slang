import "../common/camera";
import "../common/consts";
import "../common/fsquad";
import "../common/lighting";
import "../common/tonemap";

struct VertexOutput {
    float4 position       : SV_Position;
    float3 view_direction : ViewDirection;
};

float2 direction_to_rectilinear(float3 dir) {
    float x = atan2(dir.z, dir.x) / (2.0 * PI) + 0.5; // Normalize to [0, 1]
    float y = dir.y * 0.5 + 0.5; // Normalize to [0, 1]
    return float2(x, y);
}

[vk_binding(0, 0)] ConstantBuffer<Camera, Std430DataLayout> camera;
[vk_binding(1, 0)] Texture2D skybox_texture;
[vk_binding(2, 0)] SamplerState linear_sampler;

[shader("vertex")]
VertexOutput VSMain(uint index_id : SV_VertexID) {
    var vertex = generate_quad_vertex(index_id);

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
    float3 view_direction = normalize(vs_out.view_direction); // Normalize the view direction
    float2 uv = direction_to_rectilinear(view_direction);
    float4 color = skybox_texture.Sample(linear_sampler, uv); // Sample the skybox texture

    // Apply exposure
    color.rgb = ACES(color.rgb);

    var output = FragmentOutput(color);
    return output;
}
