import "../common/camera";
import "../common/consts";
import "../common/lighting";
import "../common/material";
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
    float4 view_pos : ViewPosition;
    float2 uv       : UV0;
    float3 normal   : Normal;
    nointerpolation uint64_t material_address : MaterialAddr;
};

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID,
                    uint start_instance : SV_StartInstanceLocation) {
    uint* instance_buffer = (uint*)push_constants.instance_indices_address;
    ObjectData* objects = (ObjectData*)push_constants.objects_address;
    SceneGlobals* scene = (SceneGlobals*)push_constants.scene_constants_address;

    uint iid = instance_buffer[instance_id + start_instance];
    ObjectData instance = objects[iid];

    Vertex v = pull_vertex(instance.mesh, vertex_id);

    float4 world_pos = mul(instance.model_matrix, float4(v.position, 1.0));
    float4 view_pos = mul(scene.camera.view, world_pos);
    float4 clip_pos = mul(scene.camera.projection, view_pos);

    float3x3 t_inv_model_3x3 = (float3x3) instance.transpose_inv_model_matrix;
    float3 geom_normal = normalize(mul(t_inv_model_3x3, v.normal));

    VertexOutput output;
    output.position = clip_pos;
    output.view_pos = view_pos;
    output.uv = v.uv0;
    output.normal = geom_normal;
    output.material_address = (uint64_t)instance.material;

    return output;
}

[shader("fragment")]
void FSMain(VertexOutput input) {
    if (input.material_address != 0) {
        Material* mat = (Material*)input.material_address;
        if (mat.type == MaterialType::MASK) {
            MaterialState mat_state;
            mat_state.mat = mat;
            mat_state.linear_sampler = bindless_samplers[push_constants.linear_sampler_index];

            PixelData pd;
            pd.uv = half2(input.uv);
            fetch_diffuse(mat_state, pd);

            if (pd.diffuse.w < mat.alpha_cutoff) {
                discard;
            }
        }
    }
}
