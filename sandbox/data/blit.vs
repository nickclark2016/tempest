#version 450

#pragma shader_stage(vertex)

out gl_PerVertex 
{
    vec4 gl_Position;
};

layout(location = 0) out vec2 uv_coord;

vec2 positions[4] = vec2[](
    vec2(-1.0, 1.0),
    vec2(1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(-1.0, -1.0)
);

int indices[6] = int[](
    0, 1, 2, 2, 3, 0
);

void main() {
    int index = indices[gl_VertexIndex];
    gl_Position = vec4(positions[index], 0.0, 1.0);
    uv_coord = (positions[index] + 1.0) / 2.0;
}