import "../common/mboit";
import "../common/mboit_gather";
import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    return compute_vertex(vertex_id, instance_id + base_instance);
}

[vk_binding(8, 0)] Texture2D ambient_occlusion;

[shader("fragment")]
[earlydepthstencil]
OitFragmentOutput FSMain(VertexOutput input) : OitFragmentOutput {
    var frag = compute_fragment(input, ambient_occlusion);

    beginInvocationInterlock();
    var color = gather_pass(frag.color, input.position, frag.depth);
    endInvocationInterlock();

    return OitFragmentOutput(color);
}
