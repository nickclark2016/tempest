import "mboit";

[vk_binding(6, 0)] globallycoherent RWTexture2DArray<half4> moments;
[vk_binding(7, 0)] globallycoherent RWTexture2D<float> zeroth_moment;

void quantize_moments(out float4 even_q, out float4 odd_q, float4 even, float4 odd) {
    const float4x4 mat_odd = float4x4(
         3.48044635732474, -27.5760737514826,   55.1267384344761,  -31.5311110403183,
         1.26797185782836,  -0.928755808743913, -2.07520453231032,   1.23598848322588,
        -2.1671560004294,    6.17950199592966,  -0.276515571579297, -4.23583042392097,
         0.974332879165755, -0.443426830933027, -0.360491648368785,  0.310149466050223
    );

    const float4x4 mat_even = float4x4(
         0.280504133158527, -0.757633844606942,   0.392179589334688,  -0.887531871812237,
        -2.01362265883247,   0.221551373038988,   -1.06107954265125,   2.83887201588367,
        -7.31010494985321,  13.9855979699139,     -0.114305766176437, -7.4361899359832,
        -15.8954215629556,  79.6186327084103,   -127.457278992502,    63.7349456687829
    );

    odd_q = mul(mat_odd, odd);
    even_q = mul(mat_even, even);
}

void generate_power_moments(inout float b0, inout float4 even, inout float4 odd, float depth, float transmittance) {
    float absorbance = -log(transmittance);
    absorbance = min(absorbance, 10.0); // If transmittance is zero, absorbance is infinite. Just clamp it off to a reasonable value.

    offset_moments(even, odd, -1.0);

    even *= float(b0);
    odd *= float(b0);

    // Generate new moments
    float depth2 = depth * depth;
    float depth4 = depth2 * depth2;
    float depth6 = depth4 * depth2;

    float4 new_even = float4(depth2, depth4, depth6, depth4 * depth4); // depth^(2, 4, 6, 8)
    float4 new_odd = float4(depth, depth2 * depth, depth4 * depth, depth6 * depth); // depth^(1, 3, 5, 7)

    // Quantize the float precision moments
    float4 even_q, odd_q;
    quantize_moments(even_q, odd_q, even, odd);
    
    b0 += absorbance;
    even += even_q * absorbance;
    odd += odd_q * absorbance;

    even /= float(b0);
    odd /= float(b0);

    offset_moments(even, odd, 1.0);
}

void generate_moments(float depth, float tranmittance, int2 frag_pos) {
    int3 idx0 = int3(frag_pos, 0);
    int3 idx1 = int3(frag_pos, 1);

    // Early return if the transmittance is effectively opaque
    if (tranmittance > 0.999999f) {
        discard;
    }

    float b_0 = zeroth_moment.Load(idx0.xy).x;
    float4 b_even = moments.Load(idx0);
    float4 b_odd = moments.Load(idx1);
    
    // generate power moments
    generate_power_moments(b_0, b_even, b_odd, depth, float(tranmittance));

    zeroth_moment.Store(idx0.xy, b_0);
    moments.Store(idx0, half4(b_even));
    moments.Store(idx1, half4(b_odd));
}

float4 gather_pass(float4 color, float4 frag_coord, float depth) {
    float transmittance = 1.0 - color.a;
    int2 addr = int2(frag_coord.xy);

    // Sync image access
    DeviceMemoryBarrier();
    generate_moments(depth, transmittance, addr);

    return color;
}