// shadows.slang

static const uint SHADOW_CASCADE_COUNT = 8;

struct DirectionalShadowMapParameters {
    float4x4 view_projection[SHADOW_CASCADE_COUNT];
    float cascade_splits[SHADOW_CASCADE_COUNT];
    uint cascade_count;
}

struct ShadowMapParameters {
    DirectionalShadowMapParameters directional;
    // TODO: Add other shadow types here
}