struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct RasterPushConstants
{
    float4 tint_color;
};

[[vk::push_constant]]
ConstantBuffer<RasterPushConstants> push_constants;

static const float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

static const float3 colors[3] = {
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

[shader("vertex")]
VSOutput VSMain(uint vertex_id : SV_VertexID)
{
    VSOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.color = float4(colors[vertex_id], 1.0) * push_constants.tint_color;
    return output;
}

[shader("fragment")]
float4 FSMain(VSOutput input) : SV_Target
{
    return input.color;
}
