#include "../common/camera.hlsl"
#include "../common/lighting.hlsl"
#include "../common/mesh.hlsl"
#include "../common/pulling.hlsl"

[[vk::binding(0, 0)]]
cbuffer Constants
{
    CameraData camera_data;
    DirectionalLight sun;
};

[[vk::binding(1, 0)]]
StructuredBuffer<PointLight> point_lights;

[[vk::binding(2, 0)]] StructuredBuffer<uint> vertex_pull_buffer;
[[vk::binding(3, 0)]] StructuredBuffer<Mesh> meshes;
[[vk::binding(4, 0)]] StructuredBuffer<ObjectData> objects;
[[vk::binding(5, 0)]] StructuredBuffer<uint> instance_buffer;
[[vk::binding(6, 0)]] StructuredBuffer<Material> materials;
[[vk::binding(7, 0)]] SamplerState bindless_sampler;
[[vk::binding(8, 0)]] Texture2D bindless_textures[];

struct PSInput {
    float4 position : SV_Position;
    float3 world_pos : Position;
    float2 uv : UV0;
    float3 normal : Normal;
    float3 tangent : Tangent;
    float3 bitangent : Bitangent;
    uint material_id : Material;
};

float4 get_color(Material mat, float2 uv)
{
    if (mat.base_texture == MAX_UINT)
    {
        return mat.base_texture_factor;
    }
    return bindless_textures[mat.base_texture].Sample(bindless_sampler, uv) * mat.base_texture_factor;
}

float4 normal_from_map(uint tex_id, SamplerState s, float2 uv, float3 t, float3 b, float3 n)
{
    if (tex_id == MAX_UINT)
    {
        return float4(n, 0.0);
    }

    // map rg [0, 1] -> [-1, 1]
    // map b (0.5, 1] -> [0, 1]
    float3 tan_normal = bindless_textures[tex_id].Sample(s, uv).rgb * 2.0 - 1.0;

    float3x3 tbn = float3x3(normalize(t), normalize(b), normalize(n));

    float3 normal = tan_normal.x * t + tan_normal.y * b + tan_normal.z * n;
    // float3 normal = normalize(mul(transpose(tbn), tan_normal));
    return float4(normal, 1.0);
    //return tangent.w * 0.5 + 0.5;
}

float get_roughness(uint tex_id, SamplerState s, float2 uv)
{
    if (tex_id == MAX_UINT)
    {
        return 0.0;
    } 
    return bindless_textures[tex_id].Sample(s, uv).r;
}

float get_ao(uint tex_id, SamplerState s, float2 uv)
{
    if (tex_id == MAX_UINT)
    {
        return 0.1;
    } 
    return bindless_textures[tex_id].Sample(s, uv).r;
}

float get_metallic(uint tex_id, SamplerState s, float2 uv)
{
    if (tex_id == MAX_UINT)
    {
        return 0.0;
    } 
    return bindless_textures[tex_id].Sample(s, uv).r;
}

float distribution_ggx(float3 normal, float3 half_vec, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float n_dot_h = dot(normal, half_vec);
    float n_dot_h_2 = n_dot_h * n_dot_h;
    
    float denom = n_dot_h_2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;

    return a2 / denom;
}

float geometry_schlick_ggx(float n_dot_v, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    
    return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

float geometry_smith(float3 normal, float3 view, float3 light , float roughness)
{
    float n_dot_v = max(dot(normal, view), 0.0);
    float n_dot_l = max(dot(normal, light), 0.0);
    
    return geometry_schlick_ggx(n_dot_v, roughness) * geometry_schlick_ggx(n_dot_l, roughness);
}

float3 fresnel_schlick(float cos_theta, float3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0, 1), 5);
}

PSInput VSMain(uint index_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];
    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], index_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(camera_data.view_matrix, world_pos);

    float3x3 model_3x3 = (float3x3)instance.model_matrix;
    float3x3 trans_inv_model_3x3 = (float3x3)instance.transpose_inv_model_matrix;

    PSInput result;
    result.position = mul(camera_data.proj_matrix, view_pos);
    result.world_pos = world_pos.xyz;
    result.uv = v.uv0;
    result.material_id = instance.material_id;
    result.normal = normalize(mul(trans_inv_model_3x3, v.normal));
    result.tangent = normalize(mul(model_3x3, v.tangent.xyz));
    result.bitangent = normalize(cross(result.normal, result.tangent)) * v.tangent.w;

    return result;
}

PSInput ZVSMain(uint index_id : SV_VertexID, uint instance_id : SV_InstanceID)
{
    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];
    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], index_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(camera_data.view_matrix, world_pos);

    float3x3 model_3x3 = (float3x3)instance.model_matrix;
    float3x3 trans_inv_model_3x3 = (float3x3)instance.transpose_inv_model_matrix;

    PSInput result;
    result.position = mul(camera_data.proj_matrix, view_pos);
    result.world_pos = world_pos.xyz;
    result.uv = v.uv0;
    result.material_id = instance.material_id;
    result.normal = normalize(mul(trans_inv_model_3x3, v.normal));
    result.tangent = normalize(mul(model_3x3, v.tangent.xyz));
    result.bitangent = normalize(cross(result.normal, result.tangent)) * v.tangent.w;

    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    Material mat = materials[input.material_id];


    float4 base_alpha = get_color(mat, input.uv);
    float3 base = base_alpha.rgb;

    if (base_alpha.a < 0.5) {
        discard;
    }

    float3 albedo = pow(base, 2.2);
    float3 metallic = get_metallic(mat.metallic, bindless_sampler, input.uv); 
    float3 normal = normal_from_map(mat.normal, bindless_sampler, input.uv, input.tangent, input.bitangent, input.normal).rgb;
    float3 view = normalize(camera_data.position - input.world_pos);
    float roughness = get_roughness(mat.roughness, bindless_sampler, input.uv);
    float ao = get_ao(mat.ao, bindless_sampler, input.uv);
    
    float3 f0 = 0.04;
    f0 = lerp(f0, albedo, metallic);

    float3 Lo = 0.0;

    // Directional Light
    {
        float3 light_dir = -sun.direction.rgb;
        float3 half_vec = normalize(view + light_dir);
        float3 radiance = sun.color_illum.rgb;
        float ndf = distribution_ggx(normal, half_vec, roughness);   
        float g = geometry_smith(normal, view, light_dir, roughness);      
        float3 f = fresnel_schlick(max(dot(half_vec, view), 0.0), f0);
        float3 numerator = ndf * g * f; 
        float denominator = 4.0 * max(dot(normal, view), 0.0) * max(dot(normal, light_dir), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
        float3 specular = numerator / denominator;
        float3 kS = f;
        float3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;	  
        float NdotL = max(dot(normal, light_dir), 0.0);        
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    float3 ambient = 0.03 * albedo * ao; 
    float3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + 1.0);
    // gamma correct
    color = pow(color, 1.0 / 2.2);

    return float4(color, 1.0);
}

float4 ZPSMain(PSInput input) : SV_Target0
{
    Material mat = materials[input.material_id];

    float4 base_alpha = get_color(mat, input.uv);
    float3 base = base_alpha.rgb;

    if (base_alpha.a < 0.5) {
        discard;
    }

    float2 uv_dx = ddx(input.uv);
    float2 uv_dy = ddy(input.uv);
    float3 tangent = (uv_dy.y * ddx(input.world_pos) - uv_dx.y * ddy(input.world_pos)) / (uv_dx.x * uv_dy.y - uv_dy.x * uv_dx.y);

    float4 shading_normal = normal_from_map(mat.normal, bindless_sampler, input.uv, input.tangent, input.bitangent, input.normal);

    return float4(shading_normal);
}