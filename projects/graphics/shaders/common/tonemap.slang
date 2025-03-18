float3 uncharted_2(float3 rgb) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((rgb * (A * rgb + C * B) + D * E) / (rgb * (A * rgb + B) + D * F)) - E / F;
}