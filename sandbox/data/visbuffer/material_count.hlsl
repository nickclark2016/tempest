#include "../common/mesh.hlsl"

[[vk::binding(2, 0)]] StructuredBuffer<ObjectData> object_data;
[[vk::binding(0, 1)]] globallycoherent RWStructuredBuffer<uint> material_count;
[[vk::binding(2, 1)]] Texture2D<uint2> visibility_buffer;

[numthreads(1, 1, 1)]
void CSMain(uint3 texel_loc : SV_DISPATCHTHREADID) {
    uint2 pixel = visibility_buffer.Load(texel_loc);
    uint mat_id = object_data[pixel.x].material_id;
    InterlockedAdd(material_count[mat_id], 1);
}
