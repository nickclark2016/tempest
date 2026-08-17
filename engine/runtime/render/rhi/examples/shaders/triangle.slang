struct VSOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

struct TrianglePushConstants
{
    uint64_t positions_address;
    uint64_t colors_address;
};

[[vk::push_constant]]
ConstantBuffer<TrianglePushConstants> push_constants;

[shader("vertex")]
VSOutput VSMain(uint vertex_id : SV_VertexID)
{
    float2* positions = (float2*)push_constants.positions_address;
    float3* colors = (float3*)push_constants.colors_address;

    VSOutput output;
    output.position = float4(positions[vertex_id], 0.0, 1.0);
    output.color = float4(colors[vertex_id], 1.0);
    return output;
}

[shader("fragment")]
float4 FSMain(VSOutput input) : SV_Target
{
    return input.color;
}
