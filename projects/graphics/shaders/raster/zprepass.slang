import "../common/camera";
import "../common/consts";
import "../common/lighting";
import "../common/material";
import "../common/mesh";
import "../common/scene";
import "../common/vertex_pull";

struct VertexOutput {
    float4 position    : SV_Position;
    float4 view_pos    : ViewPosition;
    float2 uv          : UV0;
    float3 normal      : Normal;
    float3 view_normal : ViewNormal;
    float3 tangent     : Tangent;
    float3 bitangent   : Bitangent;
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
[vk_binding(15, 0)] SamplerState linear_sampler;
[vk_binding(16, 0)] Texture2D bindless_textures[];

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    uint iid = instance_buffer[instance_id + base_instance];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(scene.camera.view, world_pos);
    float4 clip_pos = mul(scene.camera.projection, view_pos);

    float3x3 model_3x3 = (float3x3) instance.model_matrix;
    float3x3 t_inv_model_3x3 = (float3x3) instance.transpose_inv_model_matrix;
    float3x3 t_inv_view_3x3 = (float3x3) transpose(scene.camera.inv_view);

    float3 geom_normal = normalize(mul(t_inv_model_3x3, v.normal));
    float3 geom_tangent = normalize(mul(model_3x3, v.tangent.xyz));
    float3 geom_bitan = normalize(cross(geom_normal, geom_tangent)) * v.tangent.w;
    
    float3x3 normal_matrix = mul(t_inv_view_3x3, t_inv_model_3x3);
    float3 view_normal = normalize(mul(normal_matrix, v.normal));

    VertexOutput output;
    output.position = clip_pos;
    output.view_pos = view_pos;
    output.uv = v.uv0;
    output.normal = geom_normal;
    output.view_normal = view_normal;
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

    PixelData pd;
    pd.uv = half2(input.uv);
    pd.geom_tangent = half3(input.tangent);
    pd.geom_bitangent = half3(input.bitangent);
    pd.geom_normal = half3(input.normal);

    if (material.mat.type != MaterialType::OPAQUE) {
        fetch_diffuse(material, pd, bindless_textures);

        if (pd.diffuse.w < material.mat.alpha_cutoff) {
            discard;
        }
    }

    float3x3 upper_view = (float3x3) scene.camera.view;
    float3 view_normal = normalize(input.view_normal);
    output.encoded_normal = encode_normal(view_normal);

    return output;
}