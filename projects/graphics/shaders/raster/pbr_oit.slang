import "../common/pbr_common";

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID) {
    return compute_vertex(vertex_id, instance_id);
}

float4 to_rgbe(float3 rgb) {
    float base = max(rgb.r, max(rgb.g, rgb.b));
    int e;
    float m = frexp(base, e);
    return float4(saturate(rgb / exp2(e)), e + 127);
}

float3 from_rgbe(float4 rgbe) {
    int e = int(rgbe.a) - 127;
    return rgbe.rgb * exp2(e);
}

float4 unpack_rgba(uint packed) {
    uint4 px = uint4(packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF);
    return float4(px) / 255.0;
}

uint pack_rgba(float4 rgba) {
    uint4 px = uint4(rgba * 255.0 + 0.5);
    return px.x | (px.y << 8) | (px.z << 16) | (px.w << 24);
}

struct FragmentListNode {
    uint next;
    float depth;
    uint packed_color;
};

struct OitFragmentOutput {
    uint head : SV_Target0;
    float2 velocity : SV_Target1;
};

[shader("fragment")]
OitFragmentOutput FSMain(VertexOutput input) : OitFragmentOutput {
    FragmentOutput frag = compute_fragment(input);

    OitFragmentOutput oit_frag;
    oit_frag.head = 0;
    oit_frag.velocity = frag.velocity;

    // TODO: Implement order-independent transparency

    return oit_frag;
}
