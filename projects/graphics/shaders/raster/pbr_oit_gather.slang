import "../common/mboit";
import "../common/mboit_gather";
import "../common/pbr_common";

[vk_binding(8, 0)] globallycoherent RWByteAddressBuffer spinlock_buffer;

[shader("vertex")]
VertexOutput VSMain(uint vertex_id : SV_VertexID, uint instance_id : SV_InstanceID, uint base_instance : SV_StartInstanceLocation) {
    return compute_vertex(vertex_id, instance_id + base_instance);
}

[shader("fragment")]
[earlydepthstencil]
OitFragmentOutput FSMain(VertexOutput input) : OitFragmentOutput {
    var frag = compute_fragment(input);

    // beginInvocationInterlock();
    uint x = uint(input.position.x);
    uint y = uint(input.position.y);
    
    uint width, height;

    zeroth_moment.GetDimensions(width, height);

    uint px_index = y * width + x;

    float4 color;

    if (!IsHelperLane()) {
        bool wait = true;
        [allow_uav_condition]
        while (wait) {
            uint original;
            spinlock_buffer.InterlockedCompareExchange(px_index * 4, 0, 1, original);
            if (original == 0) {
                color = gather_pass(frag.color, input.position, frag.depth);
                AllMemoryBarrier();
                spinlock_buffer.InterlockedExchange(px_index * 4, 0);
                wait = false;
            }
        }
    }

    // beginInvocationInterlock();
    // var color = gather_pass(frag.color, input.position, frag.depth);
    // endInvocationInterlock();

    return OitFragmentOutput(color);
}
