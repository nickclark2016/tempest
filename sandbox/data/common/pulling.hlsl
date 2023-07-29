#ifndef tempest_common_pulling_hlsl
#define tempest_common_pulling_hlsl

float3 pull_vec3_f32(StructuredBuffer<uint> source, uint byte_offset)
{
    uint index = byte_offset / 4;
    uint v0 = source[byte_offset + 0];
    uint v1 = source[byte_offset + 1];
    uint v2 = source[byte_offset + 2];
    
    float3 v = {
        asfloat(v0),
        asfloat(v1),
        asfloat(v2),
    };

    return v;
}

#endif // tempest_common_pulling_hlsl