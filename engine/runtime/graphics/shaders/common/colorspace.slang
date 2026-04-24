T iec_linear_to_srgb<T>(T input) where T : __BuiltinFloatingPointType
{
    if (input <= T(0.0031308f))
    {
        return input * T(12.92f);
    }
    else
    {
        return T(1.055f) * pow(input, T(1.0f) / T(2.4f)) - T(0.055f);
    }
}

vector<T, N> iec_linear_to_srgb<T, let N : int>(vector<T, N> input) where T : __BuiltinFloatingPointType
{
    vector<T, N> result;
    for (uint i = 0; i < N; ++i)
    {
        result[i] = iec_linear_to_srgb(input[i]);
    }
    return result;
}

T iec_srgb_to_linear<T>(T input) where T : __BuiltinFloatingPointType
{
    if (input <= T(0.04045f))
    {
        return input / T(12.92f);
    }
    else
    {
        return pow((input + T(0.055f)) / T(1.055f), T(2.4f));
    }
}

vector<T, N> iec_srgb_to_linear<T, let N : int>(vector<T, N> input) where T : __BuiltinFloatingPointType
{
    vector<T, N> result;
    for (uint i = 0; i < N; ++i)
    {
        result[i] = iec_srgb_to_linear(input[i]);
    }
    return result;
}