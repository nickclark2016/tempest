#version 450

#pragma shader_stage(fragment)

#include "forward_pbr_common.glsl"

layout (location = 0) in vec3 world_coord;
layout (location = 1) in vec2 frag_uv;
layout (location = 2) in vec3 frag_normal;
layout (location = 3) in vec3 frag_tangent;
layout (location = 4) in vec3 frag_bitangent;
layout (location = 5) flat in uint material_id;

layout (set = 0, binding = 0) uniform Scene {
    Camera camera;
} scene;

layout (set = 0, binding = 1) uniform Materials {
    Material materials[material_count];
};

layout (set = 1, binding = 0) uniform sampler2D textures[bindless_count];

layout (location = 0) out vec4 frag_color;

void main(void) {
    Material mat = materials[material_id];
    vec4 albedo = texture(textures[mat.albedo_index], frag_uv);
    frag_color = albedo;
}