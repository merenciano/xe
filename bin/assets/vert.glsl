#version 460 core
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_color;

struct ShapeData {
    mat4 model;
    vec4 color;
    vec4 darkcolor;
    int albedo_idx;
    float albedo_layer;
    float pma;
    int dark_is_clip;
};

layout(std140, binding=0) uniform u_data {
    mat4 vp;
    ShapeData shape[256];
};

out Vertex {
    vec4 color;
    vec2 uv;
    flat int shape_idx;
} v_out;

void main()
{
    v_out.color = a_color;
    v_out.uv = a_uv;
    v_out.shape_idx = gl_BaseInstance;

    gl_Position = vp * shape[gl_BaseInstance].model * vec4(a_pos, 0.0, 1.0);

    if (shape[gl_BaseInstance].dark_is_clip == 1) {
        vec2 ndc = (gl_Position.xy / gl_Position.w);
        vec4 r = shape[gl_BaseInstance].darkcolor;
        float left = ndc.x - r.x;
        float right = r.z - ndc.x;
        float bottom = ndc.y - r.y;
        float up = r.w - ndc.y;

        gl_ClipDistance[0] = left;
        gl_ClipDistance[1] = right;
        gl_ClipDistance[2] = bottom;
        gl_ClipDistance[3] = up;
    }
}

