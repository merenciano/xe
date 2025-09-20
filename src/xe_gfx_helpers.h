
#ifndef XE_GFX_HELPERS_H
#define XE_GFX_HELPERS_H

#include "xe_gfx.h"
#include <llulu/lu_error.h>
#include <glad/glad.h>

static const int g_tex_fmt_lut_internal[XE_TEX_FMT_COUNT] = {
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

static const int g_tex_fmt_lut_format[XE_TEX_FMT_COUNT] = {
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

static const int g_tex_fmt_lut_type[XE_TEX_FMT_COUNT] = {
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

static inline GLenum
xe__get_gl_blend_fn(int blend_fn)
{
    lu_err_assert(blend_fn >= 0 && blend_fn < XE_BLEND_COUNT);
    return (const GLenum[]) { 0, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO }[blend_fn];
}

static inline GLenum
xe__get_gl_cull(int cull)
{
    lu_err_assert(cull >= 0 && cull < XE_CULL_COUNT);
    return (const GLenum[]) { 0, GL_FRONT, GL_BACK, GL_FRONT_AND_BACK }[cull];
}

static inline GLenum
xe__get_gl_depth_fn(int depth_fn)
{
    lu_err_assert(depth_fn >= 0 && depth_fn < XE_DEPTH_COUNT);
    return (const GLenum[]) { 0, GL_LEQUAL, GL_LESS }[depth_fn];
}

#endif /* XE_GFX_HELPERS_H */

