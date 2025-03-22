import "../common/mboit";
import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    return compute_vertex(vertex_id, instance_id + base_instance);
}

[shader("fragment")]
[earlydepthstencil]
void FSMain(VertexOutput input) : OitFragmentOutput {
    var frag = compute_fragment(input);
    float depth = compute_depth(input.current_position);

    // beginInvocationInterlock();

    // endInvocationInterlock();
}
