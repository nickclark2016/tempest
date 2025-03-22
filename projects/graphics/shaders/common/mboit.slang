import "consts";

[vk_binding(6, 0)] RasterizerOrderedTexture2DArray<half4> moments;
[vk_binding(7, 0)] RWTexture2D<half> zeroth_moment;

void offset_moments(inout half4 even, inout half4 odd, half sign) {
    odd += 0.5h * sign;
    even += half4(0.972481993925964h, 1.0h, 0.999179192513328h, 0.991778293073131h) * sign;
}

void quantize_moments(out half4 even_q, out half4 odd_q, half4 even, half4 odd) {
    const half4x4 mat_odd = half4x4(
         3.48044635732474h, -27.5760737514826h,   55.1267384344761h,  -31.5311110403183h,
         1.26797185782836h,  -0.928755808743913h, -2.07520453231032h,   1.23598848322588h,
        -2.1671560004294h,    6.17950199592966h,  -0.276515571579297h, -4.23583042392097h,
         0.974332879165755h, -0.443426830933027h, -0.360491648368785h,  0.310149466050223h
    );

    const half4x4 mat_even = half4x4(
         0.280504133158527h, -0.757633844606942h,   0.392179589334688h,  -0.887531871812237h,
        -2.01362265883247h,   0.221551373038988h,   -1.06107954265125h,   2.83887201588367h,
        -7.31010494985321h,  13.9855979699139h,     -0.114305766176437h, -7.4361899359832h,
        -15.8954215629556h,  79.6186327084103h,   -127.457278992502h,    63.7349456687829h
    );

    odd_q = mul(mat_odd, odd);
    even_q = mul(mat_even, even);
}

// TODO: Evaluate trigonometric power moments

void generate_power_moments(inout half b0, inout half4 even, inout half4 odd, half depth, half transmittance) {
    half absorbance = -log(transmittance);
    absorbance = min(absorbance, 10.0h); // If transmittance is zero, absorbance is infinite. Just clamp it off to a reasonable value.

    offset_moments(even, odd, -1.0h);

    even *= b0;
    odd *= b0;

    // Generate new moments
    half depth2 = depth * depth;
    half depth4 = depth2 * depth2;
    half depth6 = depth4 * depth2;

    half4 new_even = half4(depth2, depth4, depth6, depth4 * depth4); // depth^(2, 4, 6, 8)
    half4 new_odd = half4(depth, depth2 * depth, depth4 * depth, depth6 * depth); // depth^(1, 3, 5, 7)

    // Quantize the half precision moments
    half4 even_q, odd_q;
    quantize_moments(even_q, odd_q, even, odd);
    
    b0 += absorbance;
    even += even_q * absorbance;
    odd += odd_q * absorbance;

    even /= b0;
    odd /= b0;

    offset_moments(even, odd, 1.0h);
}

void generate_moments(float depth, float tranmittance, int2 frag_pos) {
    int3 idx0 = int3(frag_pos, 0);
    int3 idx1 = int3(frag_pos, 1);

    // Early return if the transmittance is effectively opaque
    if (tranmittance > 0.999999f) {
        discard;
    }

    half b_0 = zeroth_moment.Load(idx0.xy).x;
    half4 b_even = moments.Load(idx0);
    half4 b_odd = moments.Load(idx1);
    
    // generate power moments
    generate_power_moments(b_0, b_even, b_odd, half(depth), half(tranmittance));

    zeroth_moment.Store(idx0.xy, b_0);
    moments.Store(idx0, b_even);
    moments.Store(idx1, b_odd);
}

float4 gather_pass(float4 color, float4 frag_coord, float depth) {
    float transmittance = 1.0 - color.a;
    int2 addr = int2(frag_coord.xy);

    // Sync image access
    DeviceMemoryBarrier();
    generate_moments(depth, transmittance, addr);
    return color;
}

void resolve_pass(float4 color, float4 frag_coord, float depth) {

}