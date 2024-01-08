#include "../common/fsquad.hlsl"
#include "../common/mesh.hlsl"

[[vk::binding(0, 0)]]
cbuffer Constants
{
    float4x4 projection_matrix;
    float4x4 inv_projection_matrix;
    float4x4 view_matrix;
    float4x4 inv_view_matrix;
    float4 kernel[64];
    float2 noise_scale;
    float radius;
    float bias;
};

[[vk::binding(1, 0)]] Texture2D depth_buffer;
[[vk::binding(2, 0)]] Texture2D random_buffer;
[[vk::binding(3, 0)]] Texture2D ssao_buffer;
[[vk::binding(4, 0)]] Texture2D normal_buffer;
[[vk::binding(5, 0)]] SamplerState image_smp;
[[vk::binding(6, 0)]] SamplerState linear_smp;

float3 calculate_view_position(float2 coords)
{
    float frag_depth = depth_buffer.Sample(linear_smp, coords).r;
    float2 xy = coords * 2.0 - 1.0;
    float4 ndc = float4(xy.x, xy.y, frag_depth, 1.0);
    float4 view_pos = mul(inv_projection_matrix, ndc);
    view_pos /= view_pos.w;
    return view_pos.xyz;
}

float4 PSMain(FSQuadOut input) : SV_Target0
{
    float2 uv = input.uv;
    float3 world_pos = calculate_view_position(uv);

    float3 decoded_normal = normal_buffer.Sample(image_smp, uv).rgb;
    float3 view_normal = mul((float3x3) transpose(inv_view_matrix), decoded_normal);
    float2 scaled_noise_uv = uv * noise_scale;
    float3 random_vec = normalize(random_buffer.Sample(image_smp, scaled_noise_uv).rgb);

    float3 tangent = normalize(random_vec - view_normal * dot(random_vec, view_normal));
    float3 bitan = cross(view_normal, tangent);
    float3x3 tbn = float3x3(tangent, bitan, view_normal);

    float occlusion = 0.0;

    for (int i = 0; i < 64; ++i) {
        float3 sample_dir = mul(tbn, kernel[i].xyz);
        float3 sample_pos = world_pos + sample_dir * radius;

        float4 offset = mul(projection_matrix, float4(sample_pos, 1.0));
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sample_depth = calculate_view_position(offset.xy).z;

        float range_check = smoothstep(0.0, 1.0, radius / abs(sample_pos.z - sample_depth));
        occlusion += (sample_depth >= sample_pos.z + bias ? 1.0 : 0.0) * range_check;
    }

    float3 output = 1.0 - (occlusion / 64);
    return float4(decoded_normal, 1.0);
}

float4 BlurMain(FSQuadOut input) : SV_Target0 {
    uint width, height;
    ssao_buffer.GetDimensions(width, height);
    float2 texel_size = 1.0 / float2(width, height);
    float blur_factor = 0.0f;

    for (int t = -2; t < 2; ++t) {
        for (int s = -2; s < 2; ++s) {
            float2 offset = float2(s, t) * texel_size;
            blur_factor += ssao_buffer.Sample(image_smp, input.uv + offset).r;
        }
    }

    float3 average = blur_factor / 16;
    return float4(average, 1.0);
}