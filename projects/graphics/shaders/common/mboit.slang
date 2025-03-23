import "consts";

struct OitFragmentOutput {
    float4 transparent_surface_accumulator : SV_Target0;
};

void offset_moments(inout float4 even, inout float4 odd, float sign) {
    odd += 0.5 * sign;
    even += float4(0.972481993925964, 1.0, 0.999179192513328, 0.991778293073131) * sign;
}

float2 solve_quadratic(float3 coefficients) {
    coefficients[1] *= -0.5;

    float discriminant = coefficients[1] * coefficients[1] - coefficients[0] * coefficients[2];
    float sqrt_discriminant = sqrt(discriminant);
    if (coefficients[1] >= 0.0) {
        return float2(
            -coefficients[2] / (coefficients[1] + sqrt_discriminant),
            (-coefficients[1] - sqrt_discriminant) / coefficients[0]
        );
    } else {
        return float2(
            (-coefficients[1] + sqrt_discriminant) / coefficients[0],
            -coefficients[2] / (-coefficients[1] + sqrt_discriminant)
        );
    }
}

float find_smallest_cubic_root(float4 coefficients) {
    coefficients.xyz /= coefficients.w;
    coefficients.yz /= 3.0;

    float3 delta = float3(
        fma(-coefficients.z, coefficients.z, coefficients.y),
        fma(-coefficients.z, coefficients.y, coefficients.x),
        coefficients.z * coefficients.x - coefficients.y * coefficients.y
    );

    float discrim = 4.0 * delta.x * delta.z - delta.y * delta.y;

    float2 depressed = float2(delta.z, -coefficients.x * delta.y + 2.0 * coefficients.y * delta.z);
    float theta = abs(atan2(coefficients.x * sqrt(discrim), -depressed.y)) / 3.0;
    float2 sin_cos = float2(sin(theta), cos(theta));
    float temp = 2.0 * sqrt(-depressed.x);
    float2 x = float2(temp * sin_cos.y, temp * (-0.5h * sin_cos.y - 0.5h * sqrt(3.0) * sin_cos.x));
    float2 s = (x.x + x.y < 2.0 * coefficients.y) ? float2(-coefficients.x, x.x + coefficients.y) : float2(-coefficients.x, x.y + coefficients.y);
    
    return s.x / s.y;
}

// Solve quartic via the Neumark method (1965)
// https://people.csail.mit.edu/enikolova/project275.html
float4 solve_quartic_neumark(float coefficients[5]) {
    float b = coefficients[3] / coefficients[4];
    float c = coefficients[2] / coefficients[4];
    float d = coefficients[1] / coefficients[4];
    float e = coefficients[0] / coefficients[4];

    float p = -2.0 * c;
    float q = c * c + b * d - 4.0 * e;
    float r = d * d + b * b * e - b * c * d;

    float y = find_smallest_cubic_root(float4(r, q, p, 1.0));

    float bb = b * b;
    float fy = 4.0 * y;
    float bb_fy = bb - fy;

    float z = c - y;
    float zz = z * z;
    float fe = 4.0 * e;
    float zz_fe = zz - fe;

    float g1, g2, h1, h2;

    // Compute the coefficients of the quadratics used to solve the quartic
    // by using factorizations proposed by Herbison-Evans
    // https://cs.uwaterloo.ca/research/tr/1986/CS-86-56.pdf
    if (y < 0 || (zz + fe) * bb_fy > zz_fe * (bb + fy)) {
        float t1 = sqrt(bb_fy);
        g1 = (b + t1) * 0.5;
        g2 = (b - t1) * 0.5;

        float t2 = (b * z - 2.0 * d) / (2.0 * t1);
        h1 = fma(z, 0.5, t2);
        h2 = fma(z, 0.5, -t2);
    } else {
        float t1 = sqrt(zz_fe);
        h1 = (z + t1) * 0.5;
        h2 = (z - t1) * 0.5;

        float t2 = (b * z - 2.0 * d) / (2.0 * t1);
        g1 = fma(b, 0.5, t2);
        g2 = fma(b, 0.5, -t2);
    }

    float2 r1 = solve_quadratic(float3(1.0, g1, h1));
    float2 r2 = solve_quadratic(float3(1.0, g2, h2));

    return float4(r1, r2);
}

float compute_transmittance_at_depth(float b0, float4 even, float4 odd, float depth, float bias, float overestimation, float bias_vector[8]) {
    float b[8] = {
        odd.x,
        even.x,
        odd.y,
        even.y,
        odd.z,
        even.z,
        odd.w,
        even.w
    };

    [unroll]
    for (int i = 0; i < 8; ++i) {
        b[i] = lerp(b[i], bias_vector[i], bias);
    }

    float d22 = fma(-b[0], b[0], b[1]);
    float inv_d22 = 1.0 / d22;
    float l32_d22 = fma(-b[1], b[0], b[2]);
    float l32 = l32_d22 * inv_d22;
    float l42_d22 = fma(-b[2], b[0], b[3]);
    float l42 = l42_d22 * inv_d22;
    float l52_d22 = fma(-b[3], b[0], b[4]);
    float l52 = l52_d22 * inv_d22;

    float d33 = fma(-l32, l32_d22, fma(-b[1], b[1], b[3]));
    float inv_d33 = 1.0 / d33;
    float l43_d33 = fma(-l42, l32_d22, fma(-b[2], b[1], b[4]));
    float l43 = l43_d33 * inv_d33;
    float l53_d33 = fma(-l52, l32_d22, fma(-b[3], b[1], b[5]));
    float l53 = l53_d33 * inv_d33;

    float d44 = fma(-b[2], b[2], b[5]) - dot(float2(l42, l43), float2(l42_d22, l43_d33));
    float inv_d44 = 1.0 / d44;
    float l54_d44 = fma(-b[3], b[2], b[6]) - dot(float2(l52, l53), float2(l42_d22, l43_d33));
    float l54 = l54_d44 * inv_d44;

    float d55 = fma(-b[3], b[3], b[7]) - dot(float3(l52, l53, l54),  float3(l52_d22, l53_d33, l54_d44));
    float inv_d55 = 1.0 / d55;

    float z[5];
    z[0] = float(depth);

    float c[5];
    c[0] = 1.0;
    c[1] = z[0];
    c[2] = c[1] * z[0];
    c[3] = c[2] * z[0];
    c[4] = c[3] * z[0];

    c[1] -= b[0];
    c[2] -= fma(l32, c[1], b[1]);
    c[3] -= b[2] + dot(float2(l42, l43), float2(c[1], c[2]));
    c[4] -= b[3] + dot(float3(l52, l53, l54), float3(c[1], c[2], c[3]));

    c[1] *= inv_d22;
    c[2] *= inv_d33;
    c[3] *= inv_d44;
    c[4] *= inv_d55;

    c[3] -= l54 * c[4];
    c[2] -= dot(float2(l53, l43), float2(c[4], c[3]));
    c[1] -= dot(float3(l52, l42, l32), float3(c[4], c[3], c[2]));
    c[0] -= dot(float4(b[3], b[2], b[1], b[0]), float4(c[4], c[3], c[2], c[1]));

    // Solve the quartic equation described by the coefficients vector (c)
    float4 roots = solve_quartic_neumark(c);

    z[1] = roots.x;
    z[2] = roots.y;
    z[3] = roots.z;
    z[4] = roots.w;

    bool4 weight_multiplier = bool4(
        z[1] < z[0],
        z[2] < z[0],
        z[3] < z[0],
        z[4] < z[0]
    );

    float4 weight_factor = float4(weight_multiplier);

    // Construct interpolation polynomial
    float f0 = float(overestimation);
    float f1 = weight_factor[0];
    float f2 = weight_factor[1];
    float f3 = weight_factor[2];
    float f4 = weight_factor[3];
    float f01 = (f1  - f0) / (z[1] - z[0]);
    float f12 = (f2  - f1) / (z[2] - z[1]);
    float f23 = (f3  - f2) / (z[3] - z[2]);
    float f34 = (f4  - f3) / (z[4] - z[3]);
    float f012 = (f12 - f01) / (z[2] - z[0]);
    float f123 = (f23 - f12) / (z[3] - z[1]);
    float f234 = (f34 - f23) / (z[4] - z[2]);
    float f0123 = (f123 - f012) / (z[3] - z[0]);
    float f1234 = (f234 - f123) / (z[4] - z[1]);
    float f01234 = (f1234 - f0123) / (z[4] - z[0]);

    float poly0;
    float4 poly;
    
    poly0 = fma(-f01234, z[3], f0123);
    poly[0] = f01234;

    poly[1] = poly[0];
    poly[0] = fma(-poly[0], z[2], poly0);
    poly0 = fma(-poly0, z[2], f012);

    poly[2] = poly[1];
    poly[1] = fma(-poly[1], z[1], poly[0]);
    poly[0] = fma(-poly[0], z[1], poly0);
    poly0 = fma(-poly0, z[1], f01);

    poly[3] = poly[2];
    poly[2] = fma(-poly[2], z[0], poly[1]);
    poly[1] = fma(-poly[1], z[0], poly[0]);
    poly[0] = fma(-poly[0], z[0], poly0);
    poly0 = fma(-poly0, z[0], f0);

    float absorbance = poly0 + dot(poly, float4(b[0], b[1], b[2], b[3]));
    return saturate(exp(-float(b0) * absorbance));
}
