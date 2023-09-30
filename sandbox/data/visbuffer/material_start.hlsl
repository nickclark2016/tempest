[[vk::binding(0, 1)]] globallycoherent RWStructuredBuffer<uint> material_count;
[[vk::binding(1, 1)]] globallycoherent RWStructuredBuffer<uint> material_start;

[numthreads(1, 1, 1)]
void CSMain(uint3 material_dispatch : SV_DISPATCHTHREADID) {
    uint idx = material_dispatch.x;
    
    // Branch diverges
    uint prev;
    if (idx == 0) {
        InterlockedExchange(material_start[0], 0, prev);
    } else {
        // start[i] = material_start[i - 1] + material_count[i - 1]
        uint start = material_start[idx - 1] + material_count[idx - 1];
        InterlockedExchange(material_start[idx], start, prev);
    }
}
