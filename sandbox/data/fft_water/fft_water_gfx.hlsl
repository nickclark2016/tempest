#include "../common/camera.hlsl"
#include "../common/consts.hlsl"
#include "../common/lighting.hlsl"
#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

#define MESH_VERTEX_QUAD_X 1024
#define MESH_VERTEX_QUAD_Z 1024

[[vk::binding(0, 0)]]
cbuffer Constants
{
    CameraData camera_data;
    DirectionalLight sun;
    float4 tiling_factors;
    float4 foam_subtract;
    float4 scatter_color;
    float4 bubble_color;
    float4 foam_color;
    float normal_strength;
    float displacement_depth_attenuation;
    float far_over_near;
    float foam_depth_atten;
    float foam_roughness;
    float roughness;
    float normal_depth_atten;
    float height_modifier;
    float bubble_density;
    float wave_peak_scatter_strength;
    float scatter_strength;
    float scatter_shadow_strength;
};

[[vk::binding(1, 0)]] StructuredBuffer<uint> vertex_pull_buffer;
[[vk::binding(2, 0)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(3, 0)]] StructuredBuffer<ObjectData> instances;
[[vk::binding(4, 0)]] Texture2DArray<float4> displacement_textures;
[[vk::binding(5, 0)]] Texture2DArray<float2> slope_textures;
[[vk::binding(6, 0)]] SamplerState texture_sampler;

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TexCoord0;
    float4 world_pos : Position;
    float depth : Depth;
    float4x4 world_to_object : WorldToObj;
};

VSOutput VSMain(uint index_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instances[instance_id].mesh_id], index_id);
    VSOutput output;
    
    float4x4 model_matrix = instances[instance_id].model_matrix;
    float4x4 inv_model_matrix = instances[instance_id].inv_model_matrix;
    float4 world_pos = mul(model_matrix, float4(v.position, 1.0f));
    output.world_pos = world_pos;
    
    float4x4 vp = mul(camera_data.proj_matrix, camera_data.view_matrix);
    float4 clip_pos = mul(vp, world_pos);
    
    output.uv = v.uv0;
    output.depth = 1 + (clip_pos.z * camera_data.proj_matrix[2][2] + camera_data.proj_matrix[2][3]) / (clip_pos.w * camera_data.proj_matrix[3][2] + camera_data.proj_matrix[3][3]);

    float4 displacement_0 = displacement_textures.SampleLevel(texture_sampler, float3(v.uv0 * tiling_factors.x, 0), 0);
    float4 displacement_1 = displacement_textures.SampleLevel(texture_sampler, float3((v.uv0 - 0.5f) * tiling_factors.y, 1), 0);
    float4 displacement_2 = displacement_textures.SampleLevel(texture_sampler, float3((v.uv0 - 1.125f) * tiling_factors.y, 2), 0);
    float4 displacement_3 = displacement_textures.SampleLevel(texture_sampler, float3((v.uv0 - 1.25f) * tiling_factors.y, 3), 0);
    float4 total_displacement = displacement_0 + displacement_1 + displacement_2 + displacement_3;

    total_displacement = lerp(0.0f, total_displacement, pow(saturate(output.depth), displacement_depth_attenuation) / 12);

    v.position.xyz += mul(inv_model_matrix, float4(total_displacement.xyz, 1.0)).xyz;
    
    output.pos = mul(camera_data.proj_matrix, mul(camera_data.view_matrix, mul(model_matrix, float4(v.position, 1.0))));
    output.world_to_object = inv_model_matrix;
    return output;
}

half DotClamped(half3 a, half3 b) {
    return saturate(dot(a, b));
}

float SchlickFresnel(float3 normal, float3 viewDir)
{
				// 0.02f comes from the reflectivity bias of water kinda idk it's from a paper somewhere i'm not gonna link it tho lmaooo
    return 0.02f + (1 - 0.02f) * (pow(1 - DotClamped(normal, viewDir), 5.0f));
}

float SmithMaskingBeckmann(float3 H, float3 S, float roughness)
{
    float hdots = max(0.001f, DotClamped(H, S));
    float a = hdots / (roughness * sqrt(1 - hdots * hdots));
    float a2 = a * a;

    return a < 1.6f ? (1.0f - 1.259f * a + 0.396f * a2) / (3.535f * a + 2.181 * a2) : 0.0f;
}

float Beckmann(float ndoth, float roughness)
{
    float exp_arg = (ndoth * ndoth - 1) / (roughness * roughness * ndoth * ndoth);

    return exp(exp_arg) / (PI * roughness * roughness * ndoth * ndoth * ndoth * ndoth);
}

float4 PSMain(VSOutput input) : SV_Target
{
    float3 lightDir = -normalize(sun.direction.xyz);
    float3 viewDir = normalize(camera_data.position.xyz - input.world_pos.xyz);
    float3 halfwayDir = normalize(lightDir + viewDir);
	float depth = input.depth;
    float LdotH = DotClamped(lightDir, halfwayDir);
    float VdotH = DotClamped(viewDir, halfwayDir);
	
	
    float4 displacementFoam1 = displacement_textures.Sample(texture_sampler, float3(input.uv * tiling_factors[0], 0));
	displacementFoam1.a += foam_subtract.x;
    float4 displacementFoam2 = displacement_textures.Sample(texture_sampler, float3((input.uv - 0.5f) * tiling_factors[1], 1));
	displacementFoam2.a += foam_subtract.y;
    float4 displacementFoam3 = displacement_textures.Sample(texture_sampler, float3((input.uv - 1.125f) * tiling_factors[2], 2));
	displacementFoam3.a += foam_subtract.z;
    float4 displacementFoam4 = displacement_textures.Sample(texture_sampler, float3((input.uv - 1.25f) * tiling_factors[3], 3));
	displacementFoam4.a += foam_subtract.w;
    float4 displacementFoam = displacementFoam1 + displacementFoam2 + displacementFoam3 + displacementFoam4;

	
	float2 slopes1 = slope_textures.Sample(texture_sampler, float3(input.uv * tiling_factors.x, 0));
	float2 slopes2 = slope_textures.Sample(texture_sampler, float3((input.uv - 0.5) * tiling_factors.y, 1));
	float2 slopes3 = slope_textures.Sample(texture_sampler, float3((input.uv - 1.125) * tiling_factors.z, 2));
	float2 slopes4 = slope_textures.Sample(texture_sampler, float3((input.uv - 1.25) * tiling_factors.w, 3));
	float2 slopes = slopes1 + slopes2 + slopes3 + slopes4;

	
	slopes *= normal_strength;
	float foam = lerp(0.0f, saturate(displacementFoam.a), pow(depth, foam_depth_atten));

	float3 macroNormal = float3(0, 1, 0);
	float3 mesoNormal = normalize(float3(-slopes.x, 1.0f, -slopes.y));
	mesoNormal = normalize(lerp(float3(0, 1, 0), mesoNormal, pow(saturate(depth), normal_depth_atten)));
    mesoNormal = normalize(mul(normalize(mesoNormal), (float3x3) input.world_to_object));

    float NdotL = DotClamped(mesoNormal, lightDir);

	
	float a = roughness + foam * foam_roughness;
	float ndoth = max(0.0001f, dot(mesoNormal, halfwayDir));

	float viewMask = SmithMaskingBeckmann(halfwayDir, viewDir, a);
	float lightMask = SmithMaskingBeckmann(halfwayDir, lightDir, a);
	
	float G = rcp(1 + viewMask + lightMask);

	float eta = 1.33f;
	float R = ((eta - 1) * (eta - 1)) / ((eta + 1) * (eta + 1));
	float thetaV = acos(viewDir.y);

	float numerator = pow(1 - dot(mesoNormal, viewDir), 5 * exp(-2.69 * a));
	float F = R + (1 - R) * numerator / (1.0f + 22.7f * pow(a, 1.5f));
	F = saturate(F);
	
	float3 specular = sun.color_illum.rgb * F * G * Beckmann(ndoth, a);
    specular /= 4.0f * max(0.001f, DotClamped(macroNormal, lightDir));
    specular *= DotClamped(mesoNormal, lightDir);

    float3 envReflection = float3(0.10791, 0.10791, 0.20791);

	float H = max(0.0f, displacementFoam.y) * height_modifier;
	float3 scatterColor = scatter_color.rgb;
	float3 bubbleColor = bubble_color.rgb;
	float bubbleDensity = bubble_density;

	
    float k1 = wave_peak_scatter_strength * H * pow(DotClamped(lightDir, -viewDir), 4.0f) * pow(0.5f - 0.5f * dot(lightDir, mesoNormal), 3.0f);
    float k2 = scatter_strength * pow(DotClamped(viewDir, mesoNormal), 2.0f);
	float k3 = scatter_shadow_strength * NdotL;
	float k4 = bubbleDensity;

    float3 scatter = (k1 + k2) * scatterColor * sun.color_illum.rgb * rcp(1 + lightMask);
    scatter += k3 * scatterColor * sun.color_illum.rgb + k4 * bubbleColor * sun.color_illum.rgb;

	
	float3 output = (1 - F) * scatter + specular + F * envReflection;
	output = max(0.0f, output);
    float saturated_foam = saturate(foam);
    output = lerp(output, foam_color.rgb, saturated_foam);
    return float4(output, 1.0f);
}