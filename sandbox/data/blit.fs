#version 450

#pragma shader_stage(fragment)

layout (location = 0) in vec2 uv_coord;
layout (location = 0) out vec4 color;

layout (set = 0, binding = 0) uniform texture2D input_tex;
layout (set = 0, binding = 1) uniform sampler input_samp;

void main() {
    vec4 rgba = vec4(texture(sampler2D(input_tex, input_samp), uv_coord));
    color = rgba;
}