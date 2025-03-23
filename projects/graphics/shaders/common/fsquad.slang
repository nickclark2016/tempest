struct QuadVertex {
    float4 position;
    float2 uv;
};

QuadVertex generate_quad_vertex(uint index_id) {
    uint index = 2 - index_id;
    float2 uv = float2((index << 1) & 2, index & 2);

    QuadVertex result;
    result.position = float4(uv * 2 - 1, 0, 1);
    result.uv = float2(uv.x, uv.y);
    return result;
}