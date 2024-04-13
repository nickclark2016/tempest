[[vk_binding(0, 0)]] Texture2D input_color;
[[vk_binding(1, 0)]] Texture2D history_buffer;
[[vk_binding(2, 0)]] Texture2D velocity_buffer;
[[vk_binding(3, 0)]] SamplerState point_sampler;
[[vk_binding(4, 0)]] SamplerState linear_sampler;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv : UV;
};

[shader("vertex")]
VertexOutput VSMain(uint index_id : SV_VertexID) {
    uint index = index_id;
    float2 uv = float2((index << 1) & 2, index & 2);

    VertexOutput result;
    result.position = float4(uv * 2 - 1, 0, 1);
    result.uv = float2(uv.x, 1 - uv.y);
    return result;
}

[shader("fragment")]
float4 FSMain(VertexOutput input) : SV_Target {
    half2 velocity_sampler_position = half2(input.uv);
    half2 velocity = half2(velocity_buffer.Sample(point_sampler, velocity_sampler_position).xy);
    half2 previous_pixel_position = half2(input.uv) - velocity;

    half3 current_color = half3(input_color.Sample(point_sampler, velocity_sampler_position).xyz);
    half3 history_color = half3(history_buffer.Sample(linear_sampler, previous_pixel_position).xyz);

    // Get the color history and box filter it
    half3 right_color = half3(input_color.Sample(point_sampler, velocity_sampler_position, int2(1, 0)).xyz);
    half3 left_color = half3(input_color.Sample(point_sampler, velocity_sampler_position, int2(-1, 0)).xyz);
    half3 up_color = half3(input_color.Sample(point_sampler, velocity_sampler_position, int2(0, 1)).xyz);
    half3 down_color = half3(input_color.Sample(point_sampler, velocity_sampler_position, int2(0, -1)).xyz);

    half3 box_min = min(current_color, min(right_color, min(left_color, min(up_color, down_color))));
    half3 box_max = max(current_color, max(right_color, max(left_color, max(up_color, down_color))));

    history_color = clamp(history_color, box_min, box_max);

    float3 color = lerp(current_color, history_color, 0.9);
    return float4(color, 1.0);
}