struct ComputePushConstants
{
    uint64_t input_buffer;
    uint64_t output_buffer;
    uint32_t count;
};

[[vk::push_constant]]
ConstantBuffer<ComputePushConstants> push_constants;

[shader("compute")]
[numthreads(64, 1, 1)]
void CSMain(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x < push_constants.count)
    {
        uint* in_ptr = (uint*)push_constants.input_buffer;
        uint* out_ptr = (uint*)push_constants.output_buffer;
        out_ptr[thread_id.x] = in_ptr[thread_id.x] * in_ptr[thread_id.x] + 1;
    }
}
