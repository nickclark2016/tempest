// lighting.slang

enum LightType : uint
{
    DIRECTIONAL,
    POINT,
    SPOT
};

struct PackedU6 {
    uint4 block_0;
    uint2 block_1;

    // Operator[]
    __subscript(int index) -> uint {
        get {
            if (index < 4) {
                return block_0[index];
            } else {
                return block_1[index - 4];
            }
        }

        set {
            if (index < 4) {
                block_0[index] = newValue;
            } else {
                block_1[index - 4] = newValue;
            }
        }
    }
}

struct Light
{
    float4 color_intensity;
    float4 position_falloff;
    float4 direction;
    PackedU6 shadow_map_indices; // Up to 6 cascades, or 1 cascade per face of a cube map
    LightType type;
    uint shadow_map_count;
    bool enabled;
};
