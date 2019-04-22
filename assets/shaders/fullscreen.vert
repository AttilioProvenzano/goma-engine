#version 450

const vec2 pos[3] = {
    {-1.0, -1.0},
    {-1.0,  3.0},
    { 3.0, -1.0}
};

const vec2 uvs[3] = {
    {0.0, 0.0},
    {0.0, 2.0},
    {2.0, 0.0}
};

layout(location = 0) out vec2 outUVs;

void main() {
    gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
    outUVs = uvs[gl_VertexIndex];
}
