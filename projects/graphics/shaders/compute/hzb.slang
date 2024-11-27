struct Constants {
    uint2 image_size;
    uint num_levels;
};

[[vk_binding(0, 0)]] ConstantBuffer<Constants> constants;
[[vk_binding(1, 0)]] Texture2D<float> input_depth_buffer;
[[vk_binding(2, 0)]] RWTexture2D<float> output_depth_buffer[5];
[[vk_binding(7, 0)]] SamplerState linear_sampler;

groupshared float group_shared_depth_buffer[32 * 32];

[shader("compute")]
[numthreads(32, 32, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID, uint thread_index : SV_GroupIndex) {

#if 0

    uint2 pixel_pos = dispatch_id.xy;

    // Read the depth value from the input depth buffer
    float depth = input_depth_buffer.Sample(linear_sampler, float2(pixel_pos) / float2(constants.image_size));

    // Write the depth value to the highest level of the output depth buffer
    output_depth_buffer[0][pixel_pos] = depth;

    // Calculate the number of levels in the output depth buffer
    uint num_levels = constants.num_levels;

    // Loop over each level in the output depth buffer
    for (uint level = 1; level < num_levels; ++level) {
        // Ensure that all the threads in the group have written the previous mip level
        DeviceMemoryBarrierWithGroupSync();

        if (dispatch_id.x % (1 << level) == 0 && dispatch_id.y % (1 << level) == 0) {
            // Calculate the block position in the current level
            uint2 block_pos = pixel_pos / (1 << level);
         
            // Get level size of the current level (not power of 2)
            uint2 level_size = constants.image_size / (1 << level);
            uint2 prev_level_size = constants.image_size / (1 << (level - 1));
         
            // Get the coordinates of the 2x2 block in the previous level
            uint2 block_pos_prev = pixel_pos / (1 << (level - 1));
         
            // Read the depth values of the 2x2 block from the previous level
            float depth0 = output_depth_buffer[level - 1][block_pos_prev];
            float depth1 = output_depth_buffer[level - 1][min(block_pos_prev + uint2(1, 0), prev_level_size - 1)];
            float depth2 = output_depth_buffer[level - 1][min(block_pos_prev + uint2(0, 1), prev_level_size - 1)];
            float depth3 = output_depth_buffer[level - 1][min(block_pos_prev + uint2(1, 1), prev_level_size - 1)];
         
            // Calculate the minimum depth value of the 2x2 block
            float min_depth = min(min(depth0, depth1), min(depth2, depth3));
     
            // Write the minimum depth value to the current level of the output depth buffer
            output_depth_buffer[level][block_pos] = min_depth;
        }
    }

#else

    uint2 pixel_pos = dispatch_id.xy;

    // Read the depth value from the input depth buffer
    float depth = input_depth_buffer.Sample(linear_sampler, float2(pixel_pos) / float2(constants.image_size));

    // Write the depth value to the highest level of the output depth buffer
    output_depth_buffer[0][pixel_pos] = depth;
    group_shared_depth_buffer[thread_index] = depth;

    // Calculate the number of levels in the output depth buffer
    uint num_levels = constants.num_levels;

    // Loop over each level in the output depth buffer
    for (uint level = 1; level < num_levels; ++level) {
        // Ensure that all the threads in the group have written the previous mip level
        GroupMemoryBarrierWithGroupSync();

        if (dispatch_id.x % (1 << level) == 0 && dispatch_id.y % (1 << level) == 0) {
            // Calculate the block position in the current level
            uint2 block_pos = pixel_pos / (1 << level);
         
            // Get level size of the current level (not power of 2)
            uint2 level_size = constants.image_size / (1 << level);
            uint2 prev_level_size = constants.image_size / (1 << (level - 1));
         
            // Get the coordinates of the 2x2 block in the previous level
            uint2 block_pos_prev = pixel_pos / (1 << (level - 1));
            uint2 block_pos_prev_offset = block_pos_prev % 32;
         
            // Read the depth values of the 2x2 block from the previous level
            float depth0 = group_shared_depth_buffer[block_pos_prev_offset.x + block_pos_prev_offset.y * 32];
            float depth1 = group_shared_depth_buffer[min(block_pos_prev_offset.x + 1, 31) + block_pos_prev_offset.y * 32];
            float depth2 = group_shared_depth_buffer[block_pos_prev_offset.x + min(block_pos_prev_offset.y + 1, 31) * 32];
            float depth3 = group_shared_depth_buffer[min(block_pos_prev_offset.x + 1, 31) + min(block_pos_prev_offset.y + 1, 31) * 32];
         
            // Calculate the minimum depth value of the 2x2 block
            float min_depth = min(min(depth0, depth1), min(depth2, depth3));
     
            // Write the minimum depth value to the current level of the output depth buffer
            output_depth_buffer[level][block_pos] = min_depth;

            uint2 block_local_offset = pixel_pos % 32;
            group_shared_depth_buffer[block_local_offset.x + block_local_offset.y * 32] = depth;
        }
    }

#endif
}