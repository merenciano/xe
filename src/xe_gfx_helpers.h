
#ifndef XE_GFX_HELPERS_H
#define XE_GFX_HELPERS_H

#include "xe_gfx.h"
#include <glad/glad.h>

static const int g_tex_fmt_lut_internal[] = {
    GL_R8,
    GL_RG8,
    GL_RGB8,
    GL_SRGB8,
    GL_RGBA8,
    GL_R16F,
    GL_RG16F,
    GL_RGB16F,
    GL_RGBA16F,
    GL_R32F,
    GL_RG32F,
    GL_RGB32F,
    GL_RGBA32F,
};

static const int g_tex_fmt_lut_format[] = {
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGB,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
    GL_RED,
    GL_RG,
    GL_RGB,
    GL_RGBA,
};

static const int g_tex_fmt_lut_type[] = {
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_UNSIGNED_BYTE,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_HALF_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
    GL_FLOAT,
};

static const GLenum xe__lut_gl_blend[] = {
    0,
    0,
    GL_ONE,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_ZERO
};

static const GLenum xe__lut_gl_cull[] = {
    0,
    0,
    GL_FRONT,
    GL_BACK,
    GL_FRONT_AND_BACK
};

static const GLenum xe__lut_gl_depth_fn[] = {
    0,
    0,
    GL_LEQUAL,
    GL_LESS
};

#endif /* XE_GFX_HELPERS_H */
