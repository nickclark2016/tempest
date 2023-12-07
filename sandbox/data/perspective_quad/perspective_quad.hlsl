#include "../common/camera.hlsl"

struct SimpleVertex {
    float4 position;
    float4 uv;
};

[[vk::binding(0, 0)]] StructuredBuffer<SimpleVertex> vertex_data;
[[vk::binding(1, 0)]] cbuffer cameras {
    CameraData camera_data;
};

[[vk::binding(0, 1)]] Texture2D tex;
[[vk::binding(1, 1)]] SamplerState smp;

struct PSInput
{
    float4 pixel_position : SV_POSITION;
    float2 uv : UV0;
};

PSInput VSMain(uint index_id : SV_VERTEXID)
{
    PSInput result;
    result.pixel_position = mul(camera_data.proj_matrix, mul(camera_data.view_matrix, vertex_data[index_id].position));
    result.uv = vertex_data[index_id].uv.xy;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return tex.Sample(smp, input.uv);
}