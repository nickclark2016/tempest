import "../common/camera";
import "../common/consts";
import "../common/lighting";
import "../common/material";
import "../common/mesh";
import "../common/vertex_pull";

struct ShadowMapPushConstants {
    float4x4 view_projection;
    uint64_t objects_address;
    uint64_t instance_indices_address;
    int32_t linear_sampler_index;
    int32_t padding;
};

[[vk::push_constant]]
ConstantBuffer<ShadowMapPushConstants> push_constants;

struct VertexOutput {
    float4 position : SV_Position;
    float2 uv       : UV0;
    nointerpolation uint64_t material_address : MaterialAddr;
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
    float4 clip_pos = mul(push_constants.view_projection, world_pos);

    VertexOutput result;
    result.position = clip_pos;
    result.uv = v.uv0;
    result.material_address = (uint64_t)instance.material;
    return result;
}

[shader("fragment")]
void FSMain(VertexOutput vs_out) {
    if (vs_out.material_address != 0) {
        Material* mat = (Material*)vs_out.material_address;
        if (mat.type == MaterialType::MASK) {
            MaterialState mat_state;
            mat_state.mat = mat;
            mat_state.linear_sampler = bindless_samplers[push_constants.linear_sampler_index];

            PixelData pd;
            pd.uv = half2(vs_out.uv);
            fetch_diffuse(mat_state, pd);

            if (pd.diffuse.w < mat.alpha_cutoff) {
                discard;
            }
        }
    }
}
