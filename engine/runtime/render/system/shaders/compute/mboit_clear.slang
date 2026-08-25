import "../common/mboit";

struct MboitClearPushConstants {
    int32_t moments_storage_index;
    int32_t zeroth_moment_storage_index;
    uint2 screen_size;
};

[[vk::push_constant]]
ConstantBuffer<MboitClearPushConstants> push_constants;

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) {
    if (id.x >= push_constants.screen_size.x || id.y >= push_constants.screen_size.y) {
        return;
    }

    if (push_constants.zeroth_moment_storage_index >= 0) {
        bindless_storage_float[push_constants.zeroth_moment_storage_index][id.xy] = 0.0;
    }

    if (push_constants.moments_storage_index >= 0) {
        bindless_storage_float4_array[push_constants.moments_storage_index][uint3(id.xy, 0)] = float4(0.0);
        bindless_storage_float4_array[push_constants.moments_storage_index][uint3(id.xy, 1)] = float4(0.0);
    }
}
