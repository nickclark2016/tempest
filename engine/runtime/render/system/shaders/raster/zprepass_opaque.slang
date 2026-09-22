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
[require(spvGroupNonUniformBallot)]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID,
                    uint start_instance : SV_StartInstanceLocation) {           
    let instance_buffer = (uint*)push_constants.instance_indices_address;
    let iid = instance_buffer[instance_id + start_instance];
    let objects = (ObjectData*)push_constants.objects_address;
    let instance = &objects[iid];

    let pos = pull_position(instance.mesh, vertex_id);
    let world_pos = mul(WaveReadLaneFirst(instance.model_matrix), float4(pos, 1.0));
    let scene = (SceneGlobals*)push_constants.scene_constants_address;
    let view_pos = mul(scene.camera.view, world_pos);
    let clip_pos = mul(scene.camera.projection, view_pos);

    return VertexOutput(clip_pos);
}
