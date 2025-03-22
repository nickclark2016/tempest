import "../common/mboit";
import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    return compute_vertex(vertex_id, instance_id + base_instance);
}

struct OitFragmentOutput {
    float4 transparent_surface_accumulator : SV_Target0;
};

[shader("fragment")]
[earlydepthstencil]
OitFragmentOutput FSMain(VertexOutput input) : OitFragmentOutput {
    var frag = compute_fragment(input);

    beginInvocationInterlock();
    var color = gather_pass(frag.color, input.position, frag.depth);
    endInvocationInterlock();

    return OitFragmentOutput(color);
}
