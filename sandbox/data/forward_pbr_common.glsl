const uint material_count = 1024;
const uint bindless_count = 1024;

struct Material {
    uint albedo_index;
    uint normal_index;
    uint metal_roughness_index;
    uint ao_index;
};

struct Model {
    mat4 transformation;
    uint material_id;

    uint padding0;
    uint padding1;
    uint padding2;
};

struct Camera {
    mat4 view;
    mat4 projection;
    mat4 view_projection;
};