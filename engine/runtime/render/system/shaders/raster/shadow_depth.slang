import "../common/consts";
import "../common/material";
import "../common/mesh";
import "../common/vertex_pull";

struct ShadowDepthPushConstants {
    float4x4 light_view_proj;
    uint64_t objects_address;
    uint64_t instance_indices_address;
    int32_t linear_sampler_index;
    uint32_t padding;
};

[[vk::push_constant]]
ConstantBuffer<ShadowDepthPushConstants> push_constants;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv       : UV0;
    nointerpolation MaterialType material_type : MATERIAL_TYPE;
    nointerpolation float4 base_color_factor : BASE_COLOR_FACTOR;
    nointerpolation int base_texture_id : BASE_TEXTURE_ID;
};

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID,
                    uint start_instance : SV_StartInstanceLocation) {
    uint* instance_buffer = (uint*)push_constants.instance_indices_address;
    ObjectData* objects = (ObjectData*)push_constants.objects_address;

    uint iid = instance_buffer[instance_id + start_instance];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(instance.mesh, vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 clip_pos = mul(push_constants.light_view_proj, world_pos);

    VertexOutput output;
    output.position = clip_pos;
    output.uv = v.uv0;
    output.material_type = instance.material.type;
    output.base_color_factor = instance.material.base_texture_factor;
    output.base_texture_id = instance.material.base_texture_id;

    return output;
}

[shader("fragment")]
void FSMain(VertexOutput input) {
    if (input.material_type == MaterialType::MASK) {
        float4 base_color = input.base_color_factor;

        if (input.base_texture_id >= 0) {
            SamplerState linear_sampler = bindless_samplers[push_constants.linear_sampler_index];
            base_color *= bindless_textures[input.base_texture_id].Sample(linear_sampler, input.uv);
        }

        if (base_color.a < 0.5) { // Assuming a default alpha cutoff value
            discard;
        }
    }
}
