#version 460 core

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

layout(binding = 0) uniform sampler2DArray u_textures[16];

in Vertex {
    vec4 color;
    vec2 uv;
    flat int shape_idx;
} v_in;

out vec4 frag_color; 

void main()
{
    ShapeData mat = shape[v_in.shape_idx];
    vec4 tex = texture(u_textures[mat.albedo_idx], vec3(v_in.uv, mat.albedo_layer));

    frag_color.a = tex.a * v_in.color.a;
    frag_color.rgb = ((tex.a - 1.0) * mat.pma + 1.0 - tex.rgb) * mat.darkcolor.rgb + tex.rgb * v_in.color.rgb;
}
