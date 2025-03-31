import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    return compute_vertex(vertex_id, instance_id + base_instance);
}

[shader("fragment")]
FragmentOutput FSMain(VertexOutput input) {
    var pbr_fragment = compute_fragment(input);

    return FragmentOutput(pbr_fragment.color);
}