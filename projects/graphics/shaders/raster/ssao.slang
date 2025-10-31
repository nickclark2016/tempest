import "../common/camera";
import "../common/fsquad";
import "../common/material";
import "../common/scene";

struct SSAOParameters {
    Array<float4, 64> ssao_sample_kernel;
    float2 noise_scale;
    float radius;
    float bias;
}

[vk_binding(0, 0)] ConstantBuffer<SceneGlobals, Std430DataLayout> scene;
[vk_binding(1, 0)] ConstantBuffer<SSAOParameters, Std430DataLayout> ssao;
[vk_binding(2, 0)] Texture2D depth_buffer;
[vk_binding(3, 0)] Texture2D normal_buffer;
[vk_binding(4, 0)] Texture2D noise_texture;
[vk_binding(5, 0)] SamplerState linear_sampler;
[vk_binding(6, 0)] SamplerState point_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
}

struct FragmentOutput {
    float visibility : SV_Target0;
}

struct SSAOParams {
    float radius;
    float bias;
    float2 noise_scale;
};

[shader("vertex")]
VSOutput VSMain(uint id : SV_VertexID) {
    let vtx = generate_quad_vertex(id);
    return VSOutput(
        vtx.position,
        vtx.uv
    );
}

[shader("fragment")]
FragmentOutput FSMain(VSOutput vs_out) {
    float2 uv = vs_out.uv;
    float3 view_position = calculate_view_position(uv);
    
    float2 encoded_normal = normal_buffer.Sample(point_sampler, uv).xy; // Read the normal from the normal buffer
    float3 view_normal = decode_normal(encoded_normal);

    float2 scaled_noise_uv = ssao.noise_scale * uv;
    float2 random_dir_vec2 = normalize(noise_texture.Sample(point_sampler, scaled_noise_uv).rg);
    float3 random_dir_vec = float3(random_dir_vec2, 0.0);

    float3 tangent = normalize(random_dir_vec - view_normal * dot(random_dir_vec, view_normal));
    float3 bitangent = cross(view_normal, tangent);
    float3x3 tbn = transpose(float3x3(tangent, bitangent, view_normal)); // TBN matrix

    var occlusion_factor = 0.0;

    for (int i = 0; i < 64; ++i) {
        float3 kernel_sample = ssao.ssao_sample_kernel[i].xyz; // Sample from the kernel
        float3 sample_direction = mul(tbn, kernel_sample); // Transform the kernel sample into view space
        sample_direction *= sign(dot(sample_direction, view_normal)); // Ensure the sample is in the hemisphere of the normal
        float3 sample_position = view_position + sample_direction * ssao.radius;
        
        float4 sample_clip_pos = mul(scene.camera.projection, float4(sample_position, 1.0));
        sample_clip_pos /= sample_clip_pos.w;
        sample_clip_pos.xyz = sample_clip_pos.xyz * 0.5 + 0.5;

        float sample_depth = calculate_view_position(sample_clip_pos.xy).z;
        float depth_delta = max(abs(view_position.z - sample_depth), 1e-4);

        float range_factor = smoothstep(1.0, 0.0, ssao.radius / depth_delta);
        bool is_occluded = sample_depth < sample_position.z - ssao.bias;

        if (is_occluded) {
            occlusion_factor += range_factor;
        }
    }

    float visibility = 1.0 - (occlusion_factor / 64.0);
    visibility = clamp(visibility, 0.0, 1.0); // Ensure visibility is between 0 and 1

    return FragmentOutput(visibility);
}

float3 calculate_view_position(float2 coords) {
    float frag_depth = -depth_buffer.Sample(linear_sampler, coords).r;
    float2 xy = coords * 2.0 - 1.0;
    float4 ndc = float4(xy.x, xy.y, frag_depth, 1.0);
    float4 view_pos = mul(scene.camera.inv_projection, ndc);
    view_pos /= view_pos.w;
    return view_pos.xyz;
}
