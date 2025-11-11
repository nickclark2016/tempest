struct Constants {
    uint2 image_size;
    uint num_levels;
};

[[vk_binding(0, 0)]] Texture2D<float> input_depth_buffer;
[[vk_binding(1, 0)]] SamplerState linear_sampler;
[[vk_binding(2, 0)]] RWTexture2D<float> output_depth_buffer[];

static const uint tile_size = 32;

groupshared float group_shared_depth_buffer[tile_size];

[shader("compute")]
[numthreads(tile_size, tile_size, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID, uint thread_index : SV_GroupIndex, [vk_push_constant] uniform Constants constants) {
    let pixel_pos = dispatch_id.xy;

    // Read the depth value from the input depth buffer
    let depth = input_depth_buffer.SampleLevel(linear_sampler, float2(pixel_pos) / float2(constants.image_size), 0);

    // Write the depth value to the highest level of the output depth buffer
    output_depth_buffer[0][pixel_pos] = depth;
    group_shared_depth_buffer[thread_index] = depth;

    // Calculate the number of levels in the output depth buffer
    uint num_levels = constants.num_levels;

    // Loop over each level in the output depth buffer
    for (var level = 1u; level < num_levels; ++level) {
        // Ensure that all the threads in the group have written the previous mip level
        GroupMemoryBarrierWithGroupSync();

        if (dispatch_id.x % (1 << level) == 0 && dispatch_id.y % (1 << level) == 0) {
            // Calculate the block position in the current level
            let block_pos = pixel_pos / (1 << level);
         
            // Get level size of the current level (not power of 2)
            let level_size = constants.image_size / (1 << level);
            let prev_level_size = constants.image_size / (1 << (level - 1));
         
            // Get the coordinates of the 2x2 block in the previous level
            let block_pos_prev = pixel_pos / (1 << (level - 1));
            let block_pos_prev_offset = block_pos_prev % tile_size;
         
            // Read the depth values of the 2x2 block from the previous level
            let depth0 = group_shared_depth_buffer[block_pos_prev_offset.x + block_pos_prev_offset.y * tile_size];
            let depth1 = group_shared_depth_buffer[min(block_pos_prev_offset.x + 1, tile_size - 1) + block_pos_prev_offset.y * tile_size];
            let depth2 = group_shared_depth_buffer[block_pos_prev_offset.x + min(block_pos_prev_offset.y + 1, tile_size - 1) * tile_size];
            let depth3 = group_shared_depth_buffer[min(block_pos_prev_offset.x + 1, tile_size - 1) + min(block_pos_prev_offset.y + 1, tile_size - 1) * tile_size];
         
            // Calculate the minimum depth value of the 2x2 block
            let min_depth = max(max(depth0, depth1), max(depth2, depth3));
     
            // Write the minimum depth value to the current level of the output depth buffer
            output_depth_buffer[level][block_pos] = min_depth;

            uint2 block_local_offset = pixel_pos % tile_size;
            group_shared_depth_buffer[block_local_offset.x + block_local_offset.y * tile_size] = depth;
        }
    }
}