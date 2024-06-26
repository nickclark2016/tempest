static const float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5),
};

static const float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0),
};

struct Vertex {
    float4 position;
    float4 color;
};

[[vk::binding(0, 0)]] StructuredBuffer<Vertex> vertex_data;

struct PSInput
{
    float4 pixel_position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(uint index_id : SV_VERTEXID)
{
    PSInput result;
    result.pixel_position = vertex_data[index_id].position;
    result.color = vertex_data[index_id].color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}