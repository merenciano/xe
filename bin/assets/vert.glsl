#version 460 core
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_color;

struct ShapeData {  
    vec2 pos;
    vec2 scale;
    float rota;
    int tex_idx;
    float tex_layer;
    int pad1;
};

layout(std140, binding=0) uniform u_data {
    mat4 vp;
    ShapeData shape[126];
};

out Vertex {
    vec3 pos;
    vec2 uv;
    vec4 color;
    flat int shape_idx;
} v_out;

void main() {
    v_out.shape_idx = gl_BaseInstance;
    ShapeData data = shape[v_out.shape_idx];
    v_out.pos = vec3(a_pos.x * data.scale.x + data.pos.x, a_pos.y * data.scale.y + data.pos.y , 0.0);
    v_out.uv = a_uv;
    v_out.color = a_color;
    gl_Position = vp * vec4(v_out.pos, 1.0);
}
