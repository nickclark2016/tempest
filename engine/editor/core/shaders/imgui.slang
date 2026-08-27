[[vk::binding(0, 0)]] SamplerState bindless_samplers[];
[[vk::binding(0, 1)]] Texture2D bindless_textures[];

struct ImDrawVert
{
    float2 pos;
    float2 uv;
    uint32_t col;
};

struct PushConstants
{
    float2 scale;
    float2 translate;
    uint64_t vertex_buffer;
    uint32_t texture_id;
    uint32_t sampler_id;
};

[[vk::push_constant]]
PushConstants pc;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

[shader("vertex")]
VSOutput VSMain(uint vertex_id : SV_VertexID)
{
    uint* src = (uint*)pc.vertex_buffer;
    uint base = vertex_id * 5;

    float2 pos = { reinterpret<float>(src[base + 0]), reinterpret<float>(src[base + 1]) };
    float2 uv  = { reinterpret<float>(src[base + 2]), reinterpret<float>(src[base + 3]) };
    uint col   = src[base + 4];

    VSOutput output;
    output.position = float4(pos * pc.scale + pc.translate, 0.0, 1.0);
    output.uv = uv;
    output.color = float4(
        float(col & 0xFF),
        float((col >> 8) & 0xFF),
        float((col >> 16) & 0xFF),
        float((col >> 24) & 0xFF)
    ) / 255.0;

    return output;
}

[shader("fragment")]
float4 FSMain(VSOutput input) : SV_Target
{
    return input.color * bindless_textures[pc.texture_id].Sample(bindless_samplers[pc.sampler_id], input.uv);
}
