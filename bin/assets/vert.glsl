#version 460 core
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_color;

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

out Vertex {
    vec4 color;
    vec2 uv;
    flat int shape_idx;
} v_out;

mat3 model(ShapeData sd) {
    float c = cos(sd.angle);
    float s = sin(sd.angle);
    mat2 sc = mat2(vec2(sd.scale.x, 0.0), vec2(0.0, sd.scale.y));
    mat2 ro = mat2(vec2(c, s), vec2(-s, c));
    mat2 sr = sc * ro;
    return mat3(vec3(sr[0], 0.0), vec3(sr[1], 0.0), vec3(sd.pos, 1.0));
}

void main()
{
    v_out.color = a_color;
    v_out.uv = a_uv;
    v_out.shape_idx = gl_BaseInstance;
    ShapeData data = shape[v_out.shape_idx];
    gl_Position = vec4(mat3(vp) * model(data) * vec3(a_pos, 1.0), 1.0);
}
