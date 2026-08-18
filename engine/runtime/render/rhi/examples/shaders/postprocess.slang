// Set 2: Storage Images
[[vk::binding(0, 2)]] RWTexture2D<float4> g_storage_textures[] : register(u0, space2);

struct PostProcessPushConstants
{
    uint32_t output_image_idx;
    uint32_t width;
    uint32_t height;
    float time;
};

[[vk::push_constant]]
ConstantBuffer<PostProcessPushConstants> push_constants;

[shader("compute")]
[numthreads(8, 8, 1)]
void PostProcessCSMain(uint3 thread_id : SV_DispatchThreadID)
{
    if (thread_id.x >= push_constants.width || thread_id.y >= push_constants.height)
    {
        return;
    }

    float2 uv = float2(thread_id.xy) / float2(push_constants.width, push_constants.height);
    float2 center_offset = uv - float2(0.5, 0.5);
    float dist = length(center_offset);
    float vignette = 1.0 - smoothstep(0.35, 0.85, dist);

    float4 current = g_storage_textures[push_constants.output_image_idx][thread_id.xy];
    g_storage_textures[push_constants.output_image_idx][thread_id.xy] = float4(current.rgb * vignette, current.a);
}
