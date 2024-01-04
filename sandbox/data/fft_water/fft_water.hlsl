#include "../common/consts.hlsl"

struct WaveSpectrum
{
    float scale;
    float angle;
    float spread_blend;
    float swell;
    float alpha;
    float peak_omega;
    float gamma;
    float short_waves_fade;
};

[[vk::binding(0, 0)]]
cbuffer Constants
{
    float time_since_start;
    float delta_time;
    float gravity;
    float repeat_time;
    float damping;
    float depth;
    float low_cutoff;
    float high_cutoff;
    int seed;
    float2 wind;
    float2 lambda;
    float2 normal_strength;
    uint n;
    uint4 length_scalar;
    float foam_bias;
    float foam_decay_rate;
    float foam_add;
    float foam_threshold;
};

[[vk::binding(1, 0)]] RWTexture2DArray<float4> spectrum_textures;
[[vk::binding(2, 0)]] RWTexture2DArray<float4> initial_spectrum_textures;
[[vk::binding(3, 0)]] RWTexture2DArray<float4> displacement_textures;
[[vk::binding(4, 0)]] RWTexture2DArray<float2> slope_textures;
[[vk::binding(5, 0)]] StructuredBuffer<WaveSpectrum> spectrums;

float2 complex_multiply(float2 lhs, float2 rhs)
{
    return float2(lhs.x * rhs.x - lhs.y * rhs.y, lhs.x * rhs.y + lhs.y * rhs.x);
}

float2 euler_angle_formula(float alpha)
{
    return float2(cos(alpha), sin(alpha));
}

float hash(uint n)
{
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 0x789221u) + 0x1376312589u;
    return float(n & uint(0x7fffffffu)) / float(0x7fffffff);
}

float2 uniform_to_gaussian_dist(float u1, float u2)
{
    float rad = sqrt(-2.0f * log(u1));
    float theta = 2.0f * PI * u2;
    return float2(rad * cos(theta), rad * sin(theta));
}

float dispersion(float magnitude)
{
    return sqrt(gravity * magnitude * tanh(min(magnitude * depth, 20)));
}

float dispersion_deriv(float magnitude)
{
    float tan_h = tanh(min(magnitude * depth, 20));
    float cos_h = cosh(magnitude * depth);
    return gravity * (depth * magnitude / cos_h / cos_h + tan_h) / dispersion(magnitude) / 2.0f;
}

float normalization_factor(float f)
{
    float f2 = f * f;
    float f3 = f2 * f;
    float f4 = f3 * f;

    if (f < 5)
    {
        return -0.000564f * f4 + 0.00776f * f3 - 0.044f * f2 + 0.192f * f + 0.163f;
    }
    return -4.80e-08f * f4 + 1.07e-05f * f3 - 9.53e-04f * f2 + 5.90e-02f * f + 3.93e-01f;
}

float donelan_banner_beta(float x)
{
    if (x < 0.95f)
    {
        return 2.61f * pow(abs(x), 1.3f);
    }

    if (x < 1.6f)
    {
        return 2.28f * pow(abs(x), -1.3f);
    }

    float p = -0.4f + 0.8393f * exp(-0.567f * log(x * x));
    return pow(10.0f, p);
}

float donelan_banner(float theta, float omega, float peak_omega)
{
    float beta = donelan_banner_beta(omega / peak_omega);
    float secant_h = 1.0f / cosh(beta * theta);
    return beta / 2.0f / tanh(beta * 3.1415f) * secant_h * secant_h;
}

float cosine_normalization(float theta, float f)
{
    return normalization_factor(f) * pow(abs(cos(0.5f * theta)), 2.0f * f);
}

float spread_power(float omega, float peak_omega)
{
    if (omega > peak_omega)
    {
        return 9.77f * pow(abs(omega / peak_omega), -2.5f);
    }
    return 6.97f * pow(abs(omega / peak_omega), 5.0f);
}

float directional_spectrum(float theta, float omega, WaveSpectrum spectrum)
{
    float f = spread_power(omega, spectrum.peak_omega) + 16 * tanh(min(omega / spectrum.peak_omega, 20)) * spectrum.swell * spectrum.swell;
    return lerp(2.0f / 3.1415f * cos(theta) * cos(theta), cosine_normalization(theta - spectrum.angle, f), spectrum.spread_blend);
}

float tma_correction(float omega)
{
    float omega_h = omega * sqrt(depth / gravity);
    if (omega_h <= 1.0f)
    {
        return 0.5f * omega_h * omega_h;
    }

    if (omega_h < 2.0f)
    {
        return 1.0f - 0.5f * (2.0f - omega_h) * (2.0f - omega_h);
    }

    return 1.0f;
}

// Spectrum computation
float jonswap(float omega, WaveSpectrum spectrum)
{
    float sigma = (omega <= spectrum.peak_omega) ? 0.07f : 0.09f;
    float r = exp(-(omega - spectrum.peak_omega) * (omega - spectrum.peak_omega) / 2.0f / sigma / sigma / spectrum.peak_omega / spectrum.peak_omega);

    float inv_omega = 1.0f / omega;
    float peak_omega_ratio = spectrum.peak_omega * inv_omega;
    return spectrum.scale * tma_correction(omega) * spectrum.alpha * gravity * gravity
        * inv_omega * inv_omega * inv_omega * inv_omega * inv_omega
        * exp(-1.25f * peak_omega_ratio * peak_omega_ratio * peak_omega_ratio * peak_omega_ratio)
        * pow(abs(spectrum.gamma), r);
}

float short_waves_fade(float len, WaveSpectrum spectrum)
{
    return exp(-spectrum.short_waves_fade * spectrum.short_waves_fade * len * len);
}

float2 complex_exponent(float2 x)
{
    return float2(cos(x.y), sin(x.y) * exp(x.x));
}

[numthreads(8, 8, 1)]
void InitializeFFTState(uint3 tid : SV_DispatchThreadID)
{
    uint t_seed = tid.x + n * tid.y + n + seed;
    float4 len_scales = float4(length_scalar[0], length_scalar[1], length_scalar[2], length_scalar[3]);

    for (uint i = 0; i < 4; ++i)
    {
        float half_n = n / 2.0f;
        float delta_k = 2.0f * PI / len_scales[i];
        float2 k = (tid.xy - half_n) * delta_k;
        float k_len = length(k);

        t_seed += i + hash(t_seed) * 10;

        float4 uniform_random_samples = float4(hash(t_seed), hash(t_seed * 2), hash(t_seed * 3), hash(t_seed * 4));
        float2 gaussian_0 = uniform_to_gaussian_dist(uniform_random_samples.x, uniform_random_samples.y);
        float2 gaussian_1 = uniform_to_gaussian_dist(uniform_random_samples.z, uniform_random_samples.w);

        if (low_cutoff <= k_len && k_len <= high_cutoff)
        {
            float k_angle = atan2(k.y, k.x);
            float omega = dispersion(k_len);
            float deriv_omega_dk = dispersion_deriv(k_len);
            float js = jonswap(omega, spectrums[i * 2]);
            float ds = directional_spectrum(k_angle, omega, spectrums[i * 2]);
            float swf = short_waves_fade(k_len, spectrums[i * 2]);
            float spectrum = js * ds * swf;
            if (spectrums[i * 2 + 1].scale > 0)
            {
                spectrum += jonswap(omega, spectrums[i * 2 + 1]) * directional_spectrum(k_angle, omega, spectrums[i * 2 + 1]) * short_waves_fade(k_len, spectrums[i * 2 + 1]);
            }

            initial_spectrum_textures[uint3(tid.xy, i)] = float4(float2(gaussian_1.x, gaussian_0.y) * sqrt(2 * spectrum * abs(deriv_omega_dk) / k_len * delta_k * delta_k), 0.0f, 0.0f);
        }
        else
        {
            initial_spectrum_textures[uint3(tid.xy, i)] = 0.0f;
        }
    }
}

[numthreads(8, 8, 1)]
void PackSpectrumConjugate(uint3 tid : SV_DispatchThreadID)
{
    for (int i = 0; i < 4; ++i)
    {
        float2 h = initial_spectrum_textures[uint3(tid.xy, i)].rg;
        float2 conj = initial_spectrum_textures[uint3((n - tid.x) % n, (n - tid.y) % n, i)].rg;
        initial_spectrum_textures[uint3(tid.xy, i)] = float4(h, conj.x, -conj.y);
    }
}

[numthreads(8, 8, 1)]
void UpdateSpectrumForFFT(uint3 tid : SV_DispatchThreadID)
{
    float4 len_scales = float4(length_scalar[0], length_scalar[1], length_scalar[2], length_scalar[3]);

    for (int i = 0; i < 4; ++i)
    {
        float4 initial_signal = initial_spectrum_textures[uint3(tid.xy, i)];
        float2 h = initial_signal.xy;
        float2 conj = initial_signal.zw;

        float half_n = n / 2.0f;
        float2 k = (tid.xy - half_n) * 2.0f * PI / len_scales[i];
        float k_len = length(k);
        float k_len_rcp = rcp(k_len);

        if (k_len < 0.0001f)
        {
            k_len_rcp = 1.0f;
        }

        float w_0 = 2.0f * PI / repeat_time;
        float dispersion = floor(sqrt(gravity * k_len) / w_0) * w_0 * time_since_start;
        float2 exponent = euler_angle_formula(dispersion);
        float2 htilde = complex_multiply(h, exponent) + complex_multiply(conj, float2(exponent.x, -exponent.y));
        float2 ih = float2(-htilde.y, htilde.x);

        float2 displacement_x = ih * k.x * k_len_rcp;
        float2 displacement_y = htilde;
        float2 displacement_z = ih * k.y * k_len_rcp;

        float2 displacement_x_dx = -htilde * k.x * k.x * k_len_rcp;
        float2 displacement_y_dx = ih * k.x;
        float2 displacement_z_dx = -htilde * k.x * k.y * k_len_rcp;

        float2 displacement_y_dz = ih * k.y;
        float2 displacement_z_dz = -htilde * k.y * k.y * k_len_rcp;

        float2 htilde_displacement_x = float2(displacement_x.x - displacement_z.y, displacement_x.y + displacement_z.x);
        float2 htilde_displacement_z = float2(displacement_y.x - displacement_z_dx.y, displacement_y.y + displacement_z_dx.x);
        
        float2 htilde_slope_x = float2(displacement_y_dx.x - displacement_y_dz.y, displacement_y_dx.y + displacement_y_dz.x);
        float2 htilde_slope_z = float2(displacement_x_dx.x - displacement_z_dz.y, displacement_x_dx.y + displacement_z_dz.x);

        spectrum_textures[uint3(tid.xy, i * 2)] = float4(htilde_displacement_x, htilde_displacement_z);
        spectrum_textures[uint3(tid.xy, i * 2 + 1)] = float4(htilde_slope_x, htilde_slope_z);
    }
}

void butterfly_values(uint step, uint index, out uint2 indices, out float2 twiddle)
{
    const float two_pi = 2 * PI;
    uint b = 1024u >> (step + 1);
    uint w = b * (index / b);
    uint i = (w + index) % 1024;
    sincos(-two_pi / 1024 * w, twiddle.y, twiddle.x);
    
    twiddle.y = -twiddle.y;
    indices = uint2(i, i + b);
}

groupshared float4 g_fft_group_buffer[2][1024];

float4 compute_fft(uint tid, float4 input)
{
    g_fft_group_buffer[0][tid] = input;
    GroupMemoryBarrierWithGroupSync();
    bool flag = false;
    
    [unroll]
    for (uint step = 0; step < 10; ++step)
    {
        uint2 input_indices;
        float2 twiddle;
        
        butterfly_values(step, tid, input_indices, twiddle);
        float4 v = g_fft_group_buffer[flag][input_indices.y];
        g_fft_group_buffer[!flag][tid] = g_fft_group_buffer[flag][input_indices.x] + float4(complex_multiply(twiddle, v.xy), complex_multiply(twiddle, v.zw));
        flag = !flag;
        GroupMemoryBarrierWithGroupSync();
    }
    
    return g_fft_group_buffer[flag][tid];
}

[numthreads(1024, 1, 1)]
void HorizontalFFT(uint3 tid : SV_DispatchThreadID)
{
    for (int i = 0; i < 8; ++i)
    {
        spectrum_textures[uint3(tid.xy, i)] = compute_fft(tid.x, spectrum_textures[uint3(tid.xy, i)]);
    }
}

[numthreads(1024, 1, 1)]
void VerticalFFT(uint3 tid : SV_DispatchThreadID)
{
    for (int i = 0; i < 8; ++i)
    {
        spectrum_textures[uint3(tid.yx, i)] = compute_fft(tid.x, spectrum_textures[uint3(tid.yx, i)]);
    }
}

float4 compute_permutation(float4 data, float2 tid)
{
    return data * (1.0f - 2.0f * ((tid.x + tid.y) % 2));
}

[numthreads(8, 8, 1)]
void AssembleMaps(uint3 tid : SV_DispatchThreadID)
{
    for (int i = 0; i < 4; ++i)
    {
        float4 htilde_displacement = compute_permutation(spectrum_textures[uint3(tid.xy, 2 * i)], tid.xy);
        float4 htidle_slope = compute_permutation(spectrum_textures[uint3(tid.xy, 2 * i + 1)], tid.xy);
        
        float2 dxdz = htilde_displacement.xy;
        float2 dydxz = htilde_displacement.zw;
        float2 dyxdyz = htidle_slope.xy;
        float2 dxxdzz = htidle_slope.zw;
        
        float jacobian = (1.0f + lambda.x * dxxdzz.x) * (1.0f + lambda.y * dxxdzz.y) - lambda.x * lambda.y * dydxz.y * dydxz.y;
        float3 displacement = float3(lambda.x * dxdz.x, dydxz.x, lambda.y * dxdz.y);
        float2 slopes = dyxdyz.xy / (1 + abs(dxxdzz * lambda));
        float cov = slopes.x * slopes.y;
        
        float foam = displacement_textures[uint3(tid.xy, i)].a;
        foam *= exp(-foam_decay_rate);
        foam = saturate(foam);
        
        float biased_jacobian = max(0.0f, -(jacobian - foam_bias));
        if (biased_jacobian > foam_threshold)
        {
            foam += foam_add * biased_jacobian;
        }
        
        displacement_textures[uint3(tid.xy, i)] = float4(displacement, foam);
        slope_textures[uint3(tid.xy, i)] = float2(slopes);
    }
}
