#include "../common/mesh.hlsl"

[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> object_data;
[[vk::binding(0, 1)]] globallycoherent RWStructuredBuffer<uint> material_count;
[[vk::binding(1, 1)]] globallycoherent RWStructuredBuffer<uint> material_start;
[[vk::binding(2, 1)]] Texture2D<uint2> visibility_buffer;
[[vk::binding(4, 1)]] RWStructuredBuffer<uint2> pixel_xy_buffer;

// Assumption: material_count has been cleared
[numthreads(1, 1, 1)]
void CSMain(uint3 texel_loc : SV_DISPATCHTHREADID) {
    uint2 pixel = visibility_buffer.Load(texel_loc);
    uint mat_id = object_data[pixel.x].material_id;
    
    uint pixel_mat_id;
    InterlockedAdd(material_count[mat_id], 1, pixel_mat_id);

    uint write_idx = material_start[mat_id] + pixel_mat_id;

    pixel_xy_buffer[write_idx] = texel_loc.xy;
}