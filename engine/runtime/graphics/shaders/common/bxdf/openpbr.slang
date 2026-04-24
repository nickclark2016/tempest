import bxdf;

struct SpecularSlabResult {
    float3 reflection;
    float3 fresnel;
};

struct CoatSlabResult {
    float3 reflection;
    float fresnel;
};

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

    // ---- Slab evaluation functions ----
    // Each slab evaluates a single OpenPBR v1.1 material layer independently.
    // The main evaluate() composes them according to the spec layering model.

    // Diffuse slab: Burley (Disney) diffuse BRDF
    static float3 evaluate_diffuse_slab(DiffuseParams diffuse, float cv, float cl, float l_dot_h) {
        return diffuse.weight * diffuse.color
             * diffuse_burley(diffuse.roughness, cv, cl, l_dot_h);
    }

    // Subsurface slab: real-time approximation of subsurface scattering.
    // Uses per-channel wrap diffuse to simulate wavelength-dependent light
    // penetration (e.g. red scatters further in skin). The mean free path
    // (radius * radius_scale) controls the wrap amount per channel.
    static float3 evaluate_subsurface_slab(SubsurfaceParams subsurface, DiffuseParams diffuse, float n_dot_l) {
        let mfp = subsurface.radius * subsurface.radius_scale;
        let wrap = saturate(mfp);

        // Wrapped cosine: softens the N.L terminator per channel.
        // Energy-conserving normalization: divide by (1 + wrap)^2.
        let n_dot_l_wrap = max(float3(0.0), float3(n_dot_l) + wrap)
                            / ((1.0 + wrap) * (1.0 + wrap));

        return diffuse.weight * subsurface.color * n_dot_l_wrap / PI;
    }

    // Transmission slab: Beer-Lambert volume attenuation for thin/thick transmission
    static float3 evaluate_translucent_slab(TransmissionParams transmission) {
        var atten = transmission.color;
        if (transmission.depth > 0.0) {
            let sigma = -log(max(transmission.color, float3(1e-7))) / transmission.depth;
            atten = exp(-sigma * transmission.depth);
        }
        return atten / PI;
    }

    // Specular slab: GGX microfacet BRDF with dielectric Fresnel.
    // Returns reflection lobe and Fresnel term for layer composition.
    static SpecularSlabResult evaluate_specular_slab(SpecularParams specular, float D, float V, float v_dot_h) {
        let base_f0 = ior_to_f0(specular.ior);
        let dielectric_f0 = float3(base_f0) * specular.weight * specular.color;
        let F = F_schlick3(dielectric_f0, v_dot_h);

        var r = SpecularSlabResult();
        r.reflection = D * V * F;
        r.fresnel = F;
        return r;
    }

    // Metal slab: GGX microfacet BRDF with metallic colored Fresnel
    static float3 evaluate_metal_slab(MetalParams metal, float D, float V, float v_dot_h) {
        let F = F_schlick3(metal.color, v_dot_h);
        return D * V * F;
    }

    // Fuzz slab: Charlie NDF with Ashikhmin visibility for sheen/velvet
    static float3 evaluate_fuzz_slab(FuzzParams fuzz, float n_dot_h, float cl, float cv) {
        let D_fuzz = D_Charlie(fuzz.roughness, n_dot_h);
        let V_fuzz = V_Ashikhmin(cl, cv);
        return fuzz.color * D_fuzz * V_fuzz;
    }

    // Coat slab: independent GGX clearcoat lobe with its own normal.
    // Returns reflection lobe and scalar Fresnel for darkening.
    static CoatSlabResult evaluate_coat_slab(CoatParams coat, float3 coat_n, float3 h, float3 wi, float3 wo, float v_dot_h) {
        let coat_n_dot_h = saturate(dot(coat_n, h));
        let coat_n_dot_l = max(dot(coat_n, wi), MIN_COS);
        let coat_n_dot_v = max(dot(coat_n, wo), MIN_COS);

        let coat_alpha = max(coat.roughness * coat.roughness, 1e-4);
        let coat_D = D_GGX(coat_alpha, coat_n_dot_h, h, coat_n);
        let coat_V = V_SmithGGX(coat_alpha, coat_n_dot_v, coat_n_dot_l);
        let coat_F = F_schlick(ior_to_f0(coat.ior), 1.0, v_dot_h);

        var r = CoatSlabResult();
        r.reflection = coat.color * coat_D * coat_V * coat_F;
        r.fresnel = coat_F;
        return r;
    }

    BxDFResult evaluate(in const BxDFParams params, in const BxDFGeometry geometry, float3 wi, float3 wo, float n_dot_v, float n_dot_l) {
        var result = BxDFResult();
        result.n_dot_l = n_dot_l;

        let n = geometry.normal;
        let h = normalize(wi + wo);
        let n_dot_h = saturate(dot(n, h));
        let l_dot_h = saturate(dot(wi, h));
        let v_dot_h = saturate(dot(wo, h));

        let cv = max(n_dot_v, MIN_COS);
        let cl = max(n_dot_l, MIN_COS);

        // Shared GGX terms for specular/gloss and metal slabs
        let specular_alpha = max(params.specular.roughness * params.specular.roughness, 1e-4);
        let D = D_GGX(specular_alpha, n_dot_h, h, n);
        let V = V_SmithGGX(specular_alpha, cv, cl);

        // 1. glossy-diffuse material: layer(diffuse slab, specular slab)
        let diffuse_slab = evaluate_diffuse_slab(params.diffuse, cv, cl, l_dot_h);
        let specular_slab = evaluate_specular_slab(params.specular, D, V, v_dot_h);

        let glossy_diffuse = specular_slab.reflection + (1.0 - specular_slab.fresnel) * diffuse_slab;

        // 2. opaque-base material: mix(glossy-diffuse material, subsurface slab, subsurface weight)
        var subsurface_slab = float3(0.0);
        var opaque_base = glossy_diffuse;
        if (params.subsurface.weight > 0.0) {
            subsurface_slab = evaluate_subsurface_slab(params.subsurface, params.diffuse, n_dot_l);
            opaque_base = lerp(glossy_diffuse, subsurface_slab, params.subsurface.weight);
        }

        // 3. dielectric-base material: mix(opaque-base material, translucent slab, translucent weight)
        var transmission_slab = float3(0.0);
        var dielectric_base = opaque_base;
        if (params.transmission.weight > 0.0) {
            transmission_slab = evaluate_translucent_slab(params.transmission);
            dielectric_base = lerp(opaque_base, transmission_slab, params.transmission.weight);
        }

        // 4. base-substrate material: mix(dielectric-base material, metal slab, metal weight)
        var metal_slab = float3(0.0);
        var substrate = dielectric_base;
        if (params.metal.weight> 0.0) {
            metal_slab = evaluate_metal_slab(params.metal, D, V, v_dot_h);
            substrate = lerp(dielectric_base, metal_slab, params.metal.weight);
        }

        // 5. coat material: layer(base-substrate material, coat slab)
        var coat_fresnel = 0.0;
        var coat_reflection = float3(0.0);
        if (params.coat.weight > 0.0) {
            let coat = evaluate_coat_slab(params.coat, geometry.coat_normal, h, wi, wo, v_dot_h);
            coat_fresnel = coat.fresnel;
            coat_reflection = coat.reflection;

            float coat_atten = 1.0 - params.coat.weight * coat_fresnel * params.coat.darkening;
            substrate = substrate * coat_atten + params.coat.weight * coat_reflection;
        }

        // 6. open-pbr surface: mix(coat material, fuzz slab, fuzz weight)
        var fuzz_slab = float3(0.0);
        if (params.fuzz.weight > 0.0) {
            fuzz_slab = evaluate_fuzz_slab(params.fuzz, n_dot_h, cl, cv);
            substrate = lerp(substrate, fuzz_slab, params.fuzz.weight);
        }

        // Populate the decomposed results
        let sss_w = params.subsurface.weight;
        let trans_w = params.transmission.weight;
        let metal_w = params.metal.weight;
        let coat_atten = (params.coat.weight > 0.0)
                         ? (1.0 - params.coat.weight * coat_fresnel * params.coat.darkening)
                         : 1.0;
        let fuzz_atten = 1.0 - params.fuzz.weight;
        let outer_atten = coat_atten * fuzz_atten;

        // Glossy-diffuse is attenuated by subsurface mix, transmission mix, metal mix
        let glossy_atten = (1.0 - sss_w) * (1.0 - trans_w) * (1.0 - metal_w) * outer_atten;

        result.diffuse      = diffuse_slab * (1.0 - specular_slab.fresnel) * glossy_atten;
        result.specular     = specular_slab.reflection * glossy_atten + metal_slab * metal_w * outer_atten;
        result.subsurface   = subsurface_slab * sss_w * (1.0 - trans_w) * (1.0 - metal_w) * outer_atten;
        result.transmission = transmission_slab * trans_w * (1.0 - metal_w) * outer_atten;
        result.coat         = coat_reflection * params.coat.weight * fuzz_atten;
        result.fuzz         = fuzz_slab * params.fuzz.weight;
        result.bxdf         = substrate;

        return result;
    }
}
