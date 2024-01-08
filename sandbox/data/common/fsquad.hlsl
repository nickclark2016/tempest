#ifndef tempest_common_fsquad_hlsl
#define tempest_common_fsquad_hlsl

struct FSQuadOut {
    float4 position : SV_Position;
    float2 uv : UV;
};

FSQuadOut VSMain(uint index_id : SV_VertexID) {
    uint index = 2 - index_id;
    float2 uv = float2((index << 1) & 2, index & 2);

    FSQuadOut result;
    result.position = float4(uv * 2 - 1, 0, 1);
    result.uv = float2(uv.x, uv.y);
    return result;
}

#endif