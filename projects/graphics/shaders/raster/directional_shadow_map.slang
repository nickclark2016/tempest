import "../common/camera";
import "../common/consts";
import "../common/lighting";
import "../common/material";
import "../common/mesh";
import "../common/vertex_pull";

struct VertexOutput {
    float4 position         : SV_Position;
    float2 uv               : UV0;
    uint material_id        : Material;
};

struct FragmentOutput {
};

[vk_binding(1, 0)] StructuredBuffer<uint> vertex_pull_buffer;
[vk_binding(2, 0)] StructuredBuffer<Mesh> meshes;
[vk_binding(3, 0)] StructuredBuffer<ObjectData> objects;
[vk_binding(4, 0)] StructuredBuffer<uint> instance_buffer;
[vk_binding(5, 0)] StructuredBuffer<Material> materials;
[vk_binding(6, 0)] SamplerState linear_sampler;
[vk_binding(7, 0)] Texture2D bindless_textures[BINDLESS_TEXTURE_COUNT];

// Push constants for the shadow map
struct ShadowMapPushConstants {
    float4x4 view_projection;
};

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uniform ShadowMapPushConstants light) {
    uint iid = instance_buffer[instance_id];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(vertex_pull_buffer, meshes[instance.mesh_id], vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 clip_pos = mul(light.view_projection, world_pos);

    VertexOutput result;

    result.position = clip_pos;
    result.uv = v.uv0;
    result.material_id = instance.material_id;

    return result;
}

[shader("fragment")]
FragmentOutput FSMain(VertexOutput vs_out) {
    FragmentOutput result;

    // Only keep the fragment if the alpha value is above the cutoff
    Material material = materials[vs_out.material_id];
    if (material.base_texture_id >= 0) {
        float4 base_color = bindless_textures[(int) material.base_texture_id].Sample(linear_sampler, vs_out.uv);
        if (base_color.a < material.alpha_cutoff) {
            discard;
        }
    }

    return result;
}