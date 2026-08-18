struct HiZPushConstants {
    int32_t input_depth_index;
    int32_t point_sampler_index;
    uint32_t mip_level;
    uint32_t padding;
    uint2 src_size;
    uint2 dst_size;
};

[[vk::push_constant]]
ConstantBuffer<HiZPushConstants> push_constants;

[shader("compute")]
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID) {
    if (dispatch_id.x >= push_constants.dst_size.x || dispatch_id.y >= push_constants.dst_size.y) {
        return;
    }

    if (push_constants.input_depth_index < 0) {
        return;
    }
}
