#version 460 core

struct ShapeData {
    vec2 pos;
    vec2 scale;
    float angle;
    int tex_idx;
    float tex_layer;
    int pad1;
};

layout(std140, binding=0) uniform u_data {
    mat4 vp;
    ShapeData shape[128];
};

layout(binding = 0) uniform sampler2DArray u_textures[16];

in Vertex {
    vec4 color;
    vec2 uv;
    flat int shape_idx;
} v_in;

out vec4 frag_color;

void main() {
    frag_color = texture(u_textures[shape[v_in.shape_idx].tex_idx], vec3(v_in.uv, shape[v_in.shape_idx].tex_layer));
}
