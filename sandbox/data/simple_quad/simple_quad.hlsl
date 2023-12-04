struct Vertex {
    float4 position;
    float4 uv;
};

[[vk::binding(0, 0)]] StructuredBuffer<Vertex> vertex_data;
[[vk::binding(1, 0)]] Texture2D tex;
[[vk::binding(2, 0)]] SamplerState smp;

struct PSInput
{
    float4 pixel_position : SV_POSITION;
    float2 uv : UV0;
};

PSInput VSMain(uint index_id : SV_VERTEXID)
{
    PSInput result;
    result.pixel_position = vertex_data[index_id].position;
    result.uv = vertex_data[index_id].uv.xy;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return tex.Sample(smp, input.uv);
}