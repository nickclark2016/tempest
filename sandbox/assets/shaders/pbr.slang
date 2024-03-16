#include "camera.slang"
#include "consts.slang"
#include "lighting.slang"
#include "material.slang"
#include "mesh.slang"

struct SceneGlobals {
    Camera camera;
    Light light;
    float2 screen_size;
};

struct VertexOutput {
    float4 position       : SV_Position;
    float3 world_position : Position;
    float2 uv             : UV0;
    float3 normal         : Normal;
    float3 tangent        : Tangent;
    float3 bitangent      : Bitangent;
    uint material_id      : Material;
};

float2 pull_float2_f32(StructuredBuffer<uint> src, uint byte_offset) {
    uint index = byte_offset / 4;
    uint v0 = src[index + 0];
    uint v1 = src[index + 1];

    return {
        reinterpret<float>(v0),
        reinterpret<float>(v1),
    };
}

float3 pull_float3_f32(StructuredBuffer<uint> src, uint byte_offset) {
    uint index = byte_offset / 4;
    uint v0 = src[index + 0];
    uint v1 = src[index + 1];
    uint v2 = src[index + 2];

    return {
        reinterpret<float>(v0),
        reinterpret<float>(v1),
        reinterpret<float>(v2),
    };
}

float4 pull_float4_f32(StructuredBuffer<uint> src, uint byte_offset) {
    uint index = byte_offset / 4;
    uint v0 = src[index + 0];
    uint v1 = src[index + 1];
    uint v2 = src[index + 2];
    uint v3 = src[index + 3];


    return {
        reinterpret<float>(v0),
        reinterpret<float>(v1),
        reinterpret<float>(v2),
        reinterpret<float>(v3)
    };
}

bool has_attribute(uint attribute_offset) {
    return attribute_offset != MAX_UINT;
}

[vk_binding(0, 0)] ConstantBuffer<SceneGlobals> scene;
[vk_binding(1, 0)] StructuredBuffer<uint> vertex_pull_buffer;
[vk_binding(2, 0)] StructuredBuffer<Mesh> meshes;
[vk_binding(3, 0)] StructuredBuffer<ObjectData> objects;
[vk_binding(4, 0)] StructuredBuffer<uint> instance_buffer;
[vk_binding(5, 0)] StructuredBuffer<Material> materials;
[vk_binding(6, 0)] SamplerState linear_sampler;
[vk_binding(7, 0)] Texture2D bindless_textures[BINDLESS_TEXTURE_COUNT];

Vertex pull_vertex(StructuredBuffer<uint> src, const Mesh mesh, const uint vertex_id) {
    uint mesh_byte_offset = mesh.mesh_start_offset;
    
    Vertex v;
    v.position = pull_float3_f32(src, mesh_byte_offset + mesh.positions_offset + vertex_id * 4 * 3);
    
    uint interleave_offset = mesh_byte_offset + mesh.interleave_offset + mesh.interleave_stride * vertex_id;
    v.uv0 = pull_float2_f32(src, interleave_offset + mesh.uv_offset);
    v.normal = pull_float3_f32(src, interleave_offset + mesh.normal_offset);
    v.tangent = has_attribute(mesh.tangent_offset) ? pull_float4_f32(src, interleave_offset + mesh.tangent_offset) : float4(1, 0, 0, 1);
    v.color = has_attribute(mesh.color_offset) ? pull_float4_f32(src, interleave_offset + mesh.color_offset) : float4(1, 1, 1, 1);

    return v;
}

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(scene.camera.view, world_pos);
    float4 clip_pos = mul(scene.camera.projection, view_pos);

    float3x3 model_3x3 = (float3x3) instance.model_matrix;
    float3x3 t_inv_model_3x3 = (float3x3) instance.transpose_inv_model_matrix;

    float3 geom_normal = normalize(mul(t_inv_model_3x3, v.normal));
    float3 geom_tangent = normalize(mul(model_3x3, v.tangent.xyz));
    float3 geom_bitan = normalize(cross(geom_normal, geom_tangent)) * v.tangent.w;

    VertexOutput result;

    result.position = clip_pos;
    result.world_position = world_pos.xyz;
    result.uv = v.uv0;
    result.normal = geom_normal;
    result.tangent = geom_tangent;
    result.bitangent = geom_bitan;
    result.material_id = instance.material_id;

    return result;
}

float3 uncharted_2_tonemap(float3 x)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

[shader("fragment")]
float4 FSMain(VertexOutput input) {
    LightingState lighting;
    lighting.sun = scene.light;

    MaterialState material;
    material.mat = materials[input.material_id];
    material.linear_sampler = linear_sampler;
    material.textures = bindless_textures;

    PixelData pd;
    pd.uv = input.uv;
    pd.geom_tangent = input.tangent;
    pd.geom_bitangent = input.bitangent;
    pd.geom_normal = input.normal;

    fetch_pixel_data(material, pd);

    float4 color = evaluate_material(pd, material, lighting, scene.camera, input.world_position);
    color.rgb = uncharted_2_tonemap(color.rgb * 4.5);
    color.rgb = color.rgb * (1.0f / uncharted_2_tonemap((11.2f).xxx));

    return color;
}