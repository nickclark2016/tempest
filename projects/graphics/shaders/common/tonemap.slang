public float3 uncharted_2(float3 rgb) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((rgb * (A * rgb + C * B) + D * E) / (rgb * (A * rgb + B) + D * F)) - E / F;
}

internal static const float3x3 ACESInputMatrix = {
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

internal static const float3x3 ACESOutputMatrix = {
    { 1.60475, -0.53159, -0.07378 },
    { -0.10208, 1.10813, -0.00610 },
    { -0.00327, -0.07276, 1.07602 }
};

internal float3 RRTAndODTFit(float3 v) {
    float3 x = v * (v + 0.0245786) - 0.000090537;
    float3 y = v * (0.983729 * v + 0.432951) + 0.238081;
    return x / y;
}

internal float3 ACESFitted(float3 color) {
    color = mul(ACESInputMatrix, color);
    color = RRTAndODTFit(color);
    color = mul(ACESOutputMatrix, color);
    return saturate(color);
}

public float3 ACES(float3 color) {
    return ACESFitted(color);
}