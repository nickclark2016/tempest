import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    return compute_vertex(vertex_id, instance_id);
}

[shader("fragment")]
FragmentOutput FSMain(VertexOutput input) {
    return compute_fragment(input);
}