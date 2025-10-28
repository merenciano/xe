#include "xe_scene_internal.h"
#include "xe_gfx.h"
#include "xe_platform.h"

#include <llulu/lu_log.h>
#include <llulu/lu_error.h>

#include <stb/stb_image.h>
#include <stdint.h>

struct xe_res_arr {
    struct xe_res_image img[XE_MAX_IMAGES];
};

static struct xe_res_arr g_res;

const struct xe_res_image *
xe_image_ptr(xe_image image)
{
    uint16_t i = xe_res_index(image.id);
    return i < XE_MAX_IMAGES ? &g_res.img[i] : NULL;
}

xe_gfx_tex
xe_image_tex(xe_image image)
{
    const struct xe_res_image *img = xe_image_ptr(image);
    lu_err_assert(img);
    if (img->res.version != xe_res_version(image.id)) {
        lu_log_err("Dangling handle.");
        return (xe_gfx_tex){.idx = -1, .layer = -1};
    }

    switch (img->res.state) {
        case XE_RS_COMMITED:
            return img->tex;
        case XE_RS_STAGED:
            xe_gfx_tex_load(img->tex, img->data);
            stbi_image_free((void*)img->data);
            ((struct xe_res_image*)img)->data = NULL;
            return img->tex;

        default:
            lu_log_err("Image in invalid state.");
            lu_err_assert(0);
            break;
    }
    return (xe_gfx_tex){.idx = -1, .layer = -1};
}

static int
xe_pixel_format_from_ch(int ch)
{
    static const int formats[] = { LU_ERR_ERROR, XE_TEX_R, XE_TEX_RG, XE_TEX_RGB, XE_TEX_RGBA };
    lu_err_assert((ch > 0) && (ch < (sizeof(formats) / sizeof(*formats))) && "Out of range");
    return formats[ch];
}

static xe_image
xe_image_handle_new(void)
{
    xe_image hnd = {.id = XE_MAX_IMAGES};
    struct xe_res_image *img = NULL;
    for (int i = 0; i < XE_MAX_IMAGES; ++i) {
        if (g_res.img[i].res.state == XE_RS_FREE) {
            img = g_res.img + i;
            img->res.state = XE_RS_EMPTY;
            uint16_t ver = img->res.version++;
            hnd.id = xe_res_handle_gen(ver, i);
            break;
        }
    }

    return hnd;
}

static void
xe_image_generate_texture(struct xe_res_image *img)
{
    lu_err_assert(img && img->data);
    lu_err_assert(img->res.state == XE_RS_LOADING);
    img->tex = xe_gfx_tex_alloc((xe_gfx_texfmt){
        .width = img->w,
        .height = img->h,
        .format = xe_pixel_format_from_ch(img->c),
        .flags = 0  // Don't forward flags that prevent textures from grouping in arrays
    });
    lu_err_assert(img->tex.idx >= 0);
    img->res.state = XE_RS_STAGED;
    xe_gfx_tex_load(img->tex, img->data);
    img->res.state = XE_RS_COMMITED;
}

xe_image
xe_image_load_data(const void *pix_data, int w, int h, int c, int tex_flags)
{
    if (!pix_data) {
        lu_log_err("Could not load image from NULL pixel data.");
        return (xe_image){ .id = XE_MAX_IMAGES };
    }

    xe_image hnd = xe_image_handle_new();

    if (hnd.id != XE_MAX_IMAGES) {
        struct xe_res_image *img = (void*)xe_image_ptr(hnd);
        img->path = "";
        img->data = pix_data;
        img->res.state = XE_RS_LOADING;
        img->flags = tex_flags;
        img->w = w;
        img->h = h;
        img->c = c;
        xe_image_generate_texture(img);
        lu_err_assert(img->res.state == XE_RS_COMMITED);
    }

    return hnd;
}

xe_image
xe_image_load(const char *path, int tex_flags)
{
    if (!path || !*path) {
        lu_log_err("Can not load image from NULL or empty path.");
        return (xe_image){ .id = XE_MAX_IMAGES };
    }

    xe_image hnd = xe_image_handle_new();

    if (hnd.id != XE_MAX_IMAGES) {
        struct xe_res_image *img = (void*)xe_image_ptr(hnd);
        img->res.state = XE_RS_LOADING;
        int w, h, c;
        img->data = stbi_load(path, &w, &h, &c, 0);
        if (!img->data) {
            lu_log_err("Could not load image %s.", path);
            img->res.state = XE_RS_FAILED;
            return hnd;
        }

        img->path = path;
        img->w = w;
        img->h = h;
        img->c = c;
        img->flags = tex_flags;
        xe_image_generate_texture(img);
        lu_err_assert(img->res.state == XE_RS_COMMITED);
        stbi_image_free((stbi_uc*)img->data);
        img->data = NULL;
    }

    return hnd;
}

