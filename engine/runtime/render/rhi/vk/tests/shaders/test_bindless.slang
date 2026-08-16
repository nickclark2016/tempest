// Set 0: Samplers
[[vk::binding(0, 0)]] SamplerState        g_samplers[]         : register(s0, space0);

// Set 1: Sampled Images
[[vk::binding(0, 1)]] Texture2D<float4>   g_textures[]         : register(t0, space1);

// Set 2: Storage Images
[[vk::binding(0, 2)]] RWTexture2D<float4> g_storage_textures[] : register(u0, space2);

struct BindlessComputePushConstants
{
    uint32_t sampler_idx;
    uint32_t texture_idx;
    uint32_t width;
    uint32_t height;
    uint64_t output_buffer_address;
};

[[vk::push_constant]]
ConstantBuffer<BindlessComputePushConstants> push_constants;

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_sample(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x >= push_constants.width || thread_id.y >= push_constants.height)
    {
        return;
    }

    float2 uv = (float2(thread_id.xy) + 0.5) / float2(float(push_constants.width), float(push_constants.height));
    float4 sampled_color = g_textures[push_constants.texture_idx].SampleLevel(
        g_samplers[push_constants.sampler_idx],
        uv,
        0.0
    );

    float4* out_ptr = (float4*)push_constants.output_buffer_address;
    uint index = thread_id.y * push_constants.width + thread_id.x;
    out_ptr[index] = sampled_color;
}

struct StorageWritePushConstants
{
    uint32_t storage_image_idx;
    uint32_t width;
    uint32_t height;
};

[[vk::push_constant]]
ConstantBuffer<StorageWritePushConstants> storage_constants;

[shader("compute")]
[numthreads(8, 8, 1)]
void compute_storage_write(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x >= storage_constants.width || thread_id.y >= storage_constants.height)
    {
        return;
    }

    float r = float(thread_id.x) / float(storage_constants.width);
    float g = float(thread_id.y) / float(storage_constants.height);
    g_storage_textures[storage_constants.storage_image_idx][thread_id.xy] = float4(r, g, 0.75, 1.0);
}
