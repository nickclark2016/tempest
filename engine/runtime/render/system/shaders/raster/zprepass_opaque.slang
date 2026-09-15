import "../common/camera";
import "../common/consts";
import "../common/mesh";
import "../common/scene";
import "../common/vertex_pull";

struct ZPrepassPushConstants {
    uint64_t scene_constants_address;
    uint64_t objects_address;
    uint64_t instance_indices_address;
    int32_t linear_sampler_index;
};

[[vk::push_constant]]
ConstantBuffer<ZPrepassPushConstants> push_constants;

struct VertexOutput {
    float4 position : SV_Position;
};

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID,
                    uint start_instance : SV_StartInstanceLocation) {
    uint* instance_buffer = (uint*)push_constants.instance_indices_address;
    ObjectData* objects = (ObjectData*)push_constants.objects_address;
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;

    uint iid = instance_buffer[instance_id + start_instance];
    ObjectData* instance = &objects[iid];

    float3 pos = pull_position(instance.mesh, vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(pos, 1.0));
    float4 view_pos = mul(scene.camera.view, world_pos);
    float4 clip_pos = mul(scene.camera.projection, view_pos);

    VertexOutput output;
    output.position = clip_pos;

    return output;
}

[shader("fragment")]
[earlydepthstencil]
void FSMain() {
}
