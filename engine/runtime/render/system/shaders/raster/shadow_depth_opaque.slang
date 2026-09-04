import "../common/consts";
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

    return output;
}

[shader("fragment")]
[earlydepthstencil]
void FSMain() {
}
