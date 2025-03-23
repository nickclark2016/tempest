import "mboit";

[vk_binding(6, 0)] Texture2DArray<half4> moments;
[vk_binding(7, 0)] Texture2D<float> zeroth_moment;

void offset_and_dequantize_moments(out float4 even, out float4 odd, float4 even_q, float4 odd_q) {
    const float4x4 mat_odd = float4x4(
        -0.00482399708502382, -0.423201508674231,  0.0348312382605129, 1.67179208266592,
        -0.0233402218644408,  -0.832829097046478,  0.0193406040499625, 1.21021509068975,
        -0.010888537031885,   -0.926393772997063, -0.11723394414779,   0.983723301818275,
        -0.0308713357806732,  -0.937989172670245, -0.218033377677099,  0.845991731322996
    );

    const float4x4 mat_even = float4x4(
        -0.976220278891035, -0.456139260269401, -0.0504335521016742,   0.000838800390651085,
        -1.04828341778299,  -0.229726640510149,  0.0259608334616091,  -0.00133632693205861,
        -1.03115268628604,  -0.077844420809897,  0.00443408851014257, -0.0103744938457406,
        -0.996038443434636,  0.0175438624416783, -0.0361414253243963, -0.00317839994022725
    );

    offset_moments(even_q, odd_q, -1.0);

    odd = mul(mat_odd, odd_q);
    even = mul(mat_even, even_q);
}

float resolve_pass(float4 color, float4 frag_coord, float depth) {
    var addr = int3(int2(frag_coord.xy), 0);
    var idx0 = int4(addr.xy, 0, 0);
    var idx1 = int4(addr.xy, 1, 0);

    float transmittance_at_depth = 1.0;
    float total_transmittance = 1.0;

    float b0 = zeroth_moment.Load(addr);

    if (b0 < 0.0010050033) {
        discard;
    }

    total_transmittance = exp(-b0);

    float4 b_even_q = moments.Load(idx0);
    float4 b_odd_q = moments.Load(idx1);

    // Dequantize the moments
    float4 b_even, b_odd;
    offset_and_dequantize_moments(b_even, b_odd, b_even_q, b_odd_q);
    const float bias_vector[8] = {
        0.0,
        0.42474916387959866,
        0.0,
        0.22407802675585284,
        0.0,
        0.15369230769230768,
        0.0,
        0.12900440529089119,
    };

    transmittance_at_depth = compute_transmittance_at_depth(b0, b_even, b_odd, depth, float(2.5*1e-2), 0.1, bias_vector); 

    return transmittance_at_depth;
}