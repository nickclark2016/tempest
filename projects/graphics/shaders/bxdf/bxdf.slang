struct SpecularParams {
    float weight;
    float3 color;
    float roughness;
    float roughness_anisotropy;
    float ior;
}

struct DiffuseParams {
    float weight;
    float3 color;
    float roughness;
}

struct MetalParams {
    float weight;
    float3 color;
}

struct SubsurfaceParams {
    float weight;
    float3 color;
    float radius;
    float3 radius_scale;
    float scatter_anisotropy;
}

struct TransmissionParams {
    float weight;
    float3 color;
    float depth;
    float3 scatter;
    float scatter_anisotropy;
    float dispersion;
    float abbe_number;
}

struct ThinFilmIridescenceParams {
    float weight;
    float thickness;
    float ior;
}

struct CoatParams {
    float weight;
    float3 color;
    float roughness;
    float roughness_anisotropy;
    float ior;
    float darkening;
}

struct FuzzParams {
    float weight;
    float3 color;
    float roughness;
}

struct EmissionParams {
    float weight;
    float3 color;
}

struct GeometryParams {
    float opacity;
    bool thin_walled;
}

struct BxDFParams {
    DiffuseParams diffuse;
    SpecularParams specular;
    MetalParams metal;
    SubsurfaceParams subsurface;
    TransmissionParams transmission;
    ThinFilmIridescenceParams thin_film_iridescence;
    CoatParams coat;
    FuzzParams fuzz;
    EmissionParams emission;
    GeometryParams geometry;
}

struct BxDFResult {
    float3 bxdf;
    float3 diffuse;
    float3 specular;
    float3 coat;
    float3 transmission;
    float3 fuzz;

    float n_dot_l;
}

struct BxDFGeometry {
    float3 normal;
    float3 geometric_normal;
    float3 tangent;
    float3 bitangent;

    float3 coat_normal;
    float3 coat_tangent;
    float3 coat_bitangent;
}

interface IBxDF {
    BxDFResult evaluate(in const BxDFParams params, in const BxDFGeometry geometry, float3 wi, float3 wo, float n_dot_v, float n_dot_l);
}