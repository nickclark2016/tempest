[[vk::binding(0, 0)]] StructuredBuffer<uint> values;
[[vk::binding(1, 0)]] RWByteAddressBuffer histogram;

struct PushConstant {
    uint bin_size;
};

// Use push constants to define the bin size
[[vk::push_constant]]
PushConstant constants;

groupshared uint g_ScratchBuffer[256]; // scratch buffer

/**
 * Entrypoint for compute shader.  Computes a histogram working on 4096 bytes at a time (4 bytes per dispatch).
 *
 * @param group_index - Index of the local dispatch within the group
 * @param global_id - Indiex of the global dispatch
 */
[numthreads(1024, 1, 1)]
void CSMain(uint group_index : SV_GROUPINDEX, uint3 global_id : SV_DISPATCHTHREADID) {
    uint stride;
    uint structs;
    values.GetDimensions(structs, stride);

    // Verify the current dispatch thread isn't any larger than the size of the array
    // This allows the shader to correctly handle value arrays not sized as a multiple
    // of 4096 bytes
    if (global_id.x >= structs)
    {
        return;
    }

    uint local = group_index / 4;
    g_ScratchBuffer[local] = 0;

    // Wait for scratch buffer clear before writing
    GroupMemoryBarrierWithGroupSync();

    // Read 4 bytes
    uint value = values[global_id.x];
    uint b0 = (value >> 24) & 0xFF;
    uint b1 = (value >> 16) & 0xFF;
    uint b2 = (value >> 8) & 0xFF;
    uint b3 = (value >> 0) & 0xFF;

    // Compute the bin for each byte and increment the size of that bin in the scratch buffer
    InterlockedAdd(g_ScratchBuffer[b0 / constants.bin_size], 1);
    InterlockedAdd(g_ScratchBuffer[b1 / constants.bin_size], 1);
    InterlockedAdd(g_ScratchBuffer[b2 / constants.bin_size], 1);
    InterlockedAdd(g_ScratchBuffer[b3 / constants.bin_size], 1);

    // Wait for all writes to scratch buffer before uploading
    GroupMemoryBarrierWithGroupSync();

    // Apply this group's histograms to the global histogram
    bool is_write_eligible = (group_index % 4) == 0; // check if the local thread is aligned for a bin write
    bool is_legal_bin = local < ceil(256.0 / constants.bin_size); // check if the bin written to is in bounds
    if (is_write_eligible && is_legal_bin) {
        histogram.InterlockedAdd(group_index, g_ScratchBuffer[local]);
    }
}