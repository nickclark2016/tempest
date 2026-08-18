struct AnimatePushConstants
{
    uint64_t input_positions_address;
    uint64_t output_positions_address;
    uint64_t output_colors_address;
    float time;
    uint32_t count;
};

[[vk::push_constant]]
ConstantBuffer<AnimatePushConstants> push_constants;

[shader("compute")]
[numthreads(64, 1, 1)]
void AnimateCSMain(uint3 thread_id : SV_DispatchThreadID)
{
    uint index = thread_id.x;
    if (index >= push_constants.count)
    {
        return;
    }

    float2* in_pos = (float2*)push_constants.input_positions_address;
    float2* out_pos = (float2*)push_constants.output_positions_address;
    float3* out_col = (float3*)push_constants.output_colors_address;

    float2 base = in_pos[index];
    float t = push_constants.time;

    // Smooth continuous rotation & breathing pulse
    float angle = t * 1.2;
    float cos_a = cos(angle);
    float sin_a = sin(angle);
    float pulse = 0.85 + 0.15 * sin(t * 2.5);

    float2 rotated = float2(
        base.x * cos_a - base.y * sin_a,
        base.x * sin_a + base.y * cos_a
    ) * pulse;

    out_pos[index] = rotated;

    // Dynamic evolving RGB rainbow palette
    float offset = float(index) / float(push_constants.count);
    float phase = t * 0.75 + offset;
    out_col[index] = float3(
        0.5 + 0.5 * sin(phase * 6.28318),
        0.5 + 0.5 * sin(phase * 6.28318 + 2.094),
        0.5 + 0.5 * sin(phase * 6.28318 + 4.188)
    );
}
