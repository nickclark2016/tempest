import bxdf;

struct OpenPBRBxDF : IBxDF {
    static const float PI = 3.14159265358979323846;
    static const float MIN_COS = 1e-4;

    // ---- Utility functions ----

    static float pow5(float x) {
        float x2 = x * x;
        return x2 * x2 * x;
    }

    static float3 F_schlick3(float3 f0, float v_dot_h) {
        return f0 + (1.0 - f0) * pow5(1.0 - v_dot_h);
    }

    static float F_schlick(float f0, float f90, float v_dot_h) {
        return f0 + (f90 - f0) * pow5(1.0 - v_dot_h);
    }

    static float ior_to_f0(float ior) {
        float r = (ior - 1.0) / (ior + 1.0);
        return r * r;
    }

    // GGX NDF using cross-product formulation for numerical stability
    static float D_GGX(float alpha, float n_dot_h, float3 h, float3 n) {
        float3 nxh = cross(n, h);
        float sin2 = dot(nxh, nxh);
        float a = n_dot_h * alpha;
        float k = alpha / (sin2 + a * a);
        return k * k / PI;
    }

    // Smith height-correlated visibility: V = G / (4 * NdotL * NdotV)
    static float V_SmithGGX(float alpha, float n_dot_v, float n_dot_l) {
        float a2 = alpha * alpha;
        float ggx_v = n_dot_l * sqrt(n_dot_v * n_dot_v * (1.0 - a2) + a2);
        float ggx_l = n_dot_v * sqrt(n_dot_l * n_dot_l * (1.0 - a2) + a2);
        return 0.5 / max(ggx_v + ggx_l, 1e-7);
    }

    // Burley (Disney) diffuse BRDF
    static float diffuse_burley(float roughness, float n_dot_v, float n_dot_l, float l_dot_h) {
        float f90 = 0.5 + 2.0 * roughness * l_dot_h * l_dot_h;
        float view_scatter = F_schlick(1.0, f90, n_dot_v);
        float light_scatter = F_schlick(1.0, f90, n_dot_l);
        return view_scatter * light_scatter / PI;
    }

    // Charlie sheen NDF for fuzz/sheen layer
    static float D_Charlie(float roughness, float n_dot_h) {
        float inv_r = 1.0 / max(roughness, 0.001);
        float sin2h = max(1.0 - n_dot_h * n_dot_h, 0.0);
        return (2.0 + inv_r) * pow(sin2h, inv_r * 0.5) / (2.0 * PI);
    }

    // Neubelt (Ashikhmin) visibility for sheen
    static float V_Ashikhmin(float n_dot_l, float n_dot_v) {
        return 1.0 / (4.0 * (n_dot_l + n_dot_v - n_dot_l * n_dot_v));
    }

    // ---- Main evaluation ----

    BxDFResult evaluate(in const BxDFParams params, in const BxDFGeometry geometry, float3 wi, float3 wo, float n_dot_v, float n_dot_l) {
        BxDFResult result = {};
        result.n_dot_l = n_dot_l;

        float3 n = geometry.normal;
        float3 h = normalize(wi + wo);
        float n_dot_h = saturate(dot(n, h));
        float l_dot_h = saturate(dot(wi, h));
        float v_dot_h = saturate(dot(wo, h));

        float cv = max(n_dot_v, MIN_COS);
        float cl = max(n_dot_l, MIN_COS);

        // ============================================================
        // 1. Diffuse base (Burley)
        //    OpenPBR: base_diffuse_weight * base_color * diffuse_brdf
        // ============================================================
        float3 diffuse_brdf = params.diffuse.weight
                            * params.diffuse.color
                            * diffuse_burley(params.diffuse.roughness, cv, cl, l_dot_h);

        // ============================================================
        // 2. Transmission (replaces diffuse before specular Fresnel)
        //    Real-time approximation: volume attenuation of transmitted color
        // ============================================================
        float3 transmission_weight_color = float3(0.0);
        if (params.transmission.weight > 0.0) {
            float3 transmission_atten = params.transmission.color;
            if (params.transmission.depth > 0.0) {
                // Beer-Lambert absorption
                float3 sigma = -log(max(params.transmission.color, float3(1e-7))) / params.transmission.depth;
                transmission_atten = exp(-sigma * params.transmission.depth);
            }

            transmission_weight_color = transmission_atten / PI;
        }

        // Inner base: diffuse blended with transmission
        float3 inner_base = lerp(diffuse_brdf, transmission_weight_color, params.transmission.weight);

        // ============================================================
        // 3. Specular dielectric layer (GGX microfacet over inner base)
        //    OpenPBR: fresnel_mix(specular_brdf, inner_base, specular_ior)
        // ============================================================
        float specular_alpha = max(params.specular.roughness * params.specular.roughness, 1e-4);

        float D = D_GGX(specular_alpha, n_dot_h, h, n);
        float V = V_SmithGGX(specular_alpha, cv, cl);

        // Dielectric F0 from IOR, scaled by specular weight and color
        float base_f0 = ior_to_f0(params.specular.ior);
        float3 dielectric_f0 = float3(base_f0) * params.specular.weight * params.specular.color;
        float3 F_dielectric = F_schlick3(dielectric_f0, v_dot_h);

        float3 specular_dielectric = D * V * F_dielectric;

        // Fresnel mix: specular reflects, remainder transmits to inner base
        float3 dielectric_substrate = specular_dielectric + (1.0 - F_dielectric) * inner_base;

        // ============================================================
        // 4. Metal layer
        //    OpenPBR: metal_weight blend between dielectric and metal
        //    Metal F0 = metal_color (colored Fresnel)
        // ============================================================
        float3 F_metal = F_schlick3(params.metal.color, v_dot_h);
        float3 metal_substrate = D * V * F_metal;

        float3 substrate = lerp(dielectric_substrate, metal_substrate, params.metal.weight);

        // ============================================================
        // 5. Fuzz layer (sheen, Charlie NDF)
        //    OpenPBR: fuzz_weight * fuzz_brdf + (1 - fuzz_weight) * substrate
        // ============================================================
        float3 fuzz_brdf = float3(0.0);
        if (params.fuzz.weight > 0.0) {
            float D_fuzz = D_Charlie(params.fuzz.roughness, n_dot_h);
            float V_fuzz = V_Ashikhmin(cl, cv);
            fuzz_brdf = params.fuzz.color * D_fuzz * V_fuzz;

            substrate = lerp(substrate, fuzz_brdf, params.fuzz.weight);
        }

        // ============================================================
        // 6. Coat layer (clearcoat, independent GGX lobe)
        //    OpenPBR: coat_weight * fresnel * coat_brdf on top, with
        //    energy darkening of the base beneath
        // ============================================================
        float3 coat_brdf = float3(0.0);
        if (params.coat.weight > 0.0) {
            float3 coat_n = geometry.coat_normal;
            float coat_n_dot_h = saturate(dot(coat_n, h));
            float coat_n_dot_l = max(dot(coat_n, wi), MIN_COS);
            float coat_n_dot_v = max(dot(coat_n, wo), MIN_COS);

            float coat_alpha = max(params.coat.roughness * params.coat.roughness, 1e-4);
            float coat_D = D_GGX(coat_alpha, coat_n_dot_h, h, coat_n);
            float coat_V = V_SmithGGX(coat_alpha, coat_n_dot_v, coat_n_dot_l);
            float coat_f0 = ior_to_f0(params.coat.ior);
            float coat_F = F_schlick(coat_f0, 1.0, v_dot_h);

            coat_brdf = params.coat.color * coat_D * coat_V * coat_F;

            // Darken base layers: energy absorbed by the coat
            float coat_attenuation = 1.0 - params.coat.weight * coat_F * params.coat.darkening;
            substrate *= coat_attenuation;

            // Add coat reflection
            substrate += params.coat.weight * coat_brdf;
        }

        // ============================================================
        // Populate decomposed results
        // ============================================================
        float metal_atten = 1.0 - params.metal.weight;
        float fuzz_atten = 1.0 - params.fuzz.weight;
        float coat_atten = (params.coat.weight > 0.0)
                         ? (1.0 - params.coat.weight * F_schlick(ior_to_f0(params.coat.ior), 1.0, v_dot_h) * params.coat.darkening)
                         : 1.0;

        result.diffuse      = inner_base * (1.0 - F_dielectric) * metal_atten * fuzz_atten * coat_atten;
        result.specular     = (specular_dielectric * metal_atten + metal_substrate * params.metal.weight) * fuzz_atten * coat_atten;
        result.transmission = transmission_weight_color * params.transmission.weight * (1.0 - F_dielectric) * metal_atten * fuzz_atten * coat_atten;
        result.fuzz         = fuzz_brdf * params.fuzz.weight * coat_atten;
        result.coat         = coat_brdf * params.coat.weight;
        result.bxdf         = substrate;

        return result;
    }
}
