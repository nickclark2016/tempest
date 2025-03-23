import "../common/mboit";
import "../common/mboit_resolve";
import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    return compute_vertex(vertex_id, instance_id + base_instance);
}

[shader("fragment")]
[earlydepthstencil]
OitFragmentOutput FSMain(VertexOutput input) : OitFragmentOutput {
    var frag = compute_fragment(input);
    
    float transmittance = resolve_pass(frag.color, input.position, frag.depth);    
    float4 color = float4(frag.color.rgb * frag.color.a * transmittance, frag.color.a * transmittance);

    return OitFragmentOutput(color);
}
