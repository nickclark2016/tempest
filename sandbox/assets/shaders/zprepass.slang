#include "camera.slang"
#include "consts.slang"
#include "material.slang"
#include "mesh.slang"
#include "vertex_pull.slang"

struct SceneGlobals {
    Camera camera;
    float2 screen_size;
    float4 ambient_light;
    float4 jitter_offset;
    Light sun;
};

struct VertexOutput {
    float4 position  : SV_Position;
    float2 uv        : UV0;
    float3 normal    : Normal;
    float3 tangent   : Tangent;
    float3 bitangent : Bitangent;
    uint material_id;
};

struct FragmentOutput {
    float2 encoded_normal : SV_Target0;
};

[vk_binding(0, 0)] ConstantBuffer<SceneGlobals> scene;
[vk_binding(1, 0)] StructuredBuffer<uint> vertex_pull_buffer;
[vk_binding(2, 0)] StructuredBuffer<Mesh> meshes;
[vk_binding(3, 0)] StructuredBuffer<ObjectData> objects;
[vk_binding(4, 0)] StructuredBuffer<uint> instance_buffer;
[vk_binding(5, 0)] StructuredBuffer<Material> materials;
[vk_binding(6, 0)] SamplerState linear_sampler;
[vk_binding(7, 0)] Texture2D bindless_textures[BINDLESS_TEXTURE_COUNT];

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

    VertexOutput output;
    output.position = clip_pos + scene.jitter_offset;
    output.uv = v.uv0;
    output.normal = geom_normal;
    output.tangent = geom_tangent;
    output.bitangent = geom_bitan;
    output.material_id = instance.material_id;

    return output;
}

[shader("fragment")]
FragmentOutput FSMain(VertexOutput input) {
    FragmentOutput output;

    MaterialState material;
    material.mat = materials[input.material_id];
    material.linear_sampler = linear_sampler;
    material.textures = bindless_textures;

    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);

    fetch_pixel_data(material, pd);

    if (pd.diffuse.w < material.mat.alpha_cutoff) {
        discard;
    }

    output.encoded_normal = encode_normal(pd.shading_normal);

    return output;
}