#version 450

#pragma shader_stage(vertex)

#include "forward_pbr_common.glsl"

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 tangent;

layout (location = 0) out vec3 world_coord;
layout (location = 1) out vec2 frag_uv;
layout (location = 2) out vec3 frag_normal;
layout (location = 3) out vec3 frag_tangent;
layout (location = 4) out vec3 frag_bitangent;
layout (location = 5) flat out uint material_id;

layout (set = 0, binding = 0) uniform Scene {
    Camera camera;
} scene;

layout (set = 0, binding = 2) buffer ModelData {
    Model models[];
};

void main(void) {
    Model model = models[gl_InstanceIndex];
    vec4 world = model.transformation * vec4(position, 1.0);
    gl_Position = scene.camera.view_projection * world;

    world_coord = world.xyz;
    frag_uv = uv;
    material_id = model.material_id;
}
