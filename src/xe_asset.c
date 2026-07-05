#include <xe_asset.h>
#include "xe_scene_internal.h"
#include "xe_render.h"
#include "xe_platform.h"

#include <llulu/lu_log.h>
#include <llulu/lu_error.h>

#include <stb/stb_image.h>
#include <stdint.h>

struct xe_asset_arr {
    xe_asset_image img[XE_MAX_IMAGES];
    xe_asset_pipeline pipelines[XE_MAX_PIPELINES];
};

static struct xe_asset_arr g_assets;

const xe_asset_image *
xe_asset_image_data(xe_image image)
{
    uint16_t i = xe_handle_index(image.id);
    return i < XE_MAX_IMAGES ? &g_assets.img[i] : NULL;
}

xe_tex
xe_image_tex(xe_image image)
{
    const xe_asset_image *img = xe_asset_image_data(image);
    lu_err_assert(img);
    if (img->asset.version != xe_handle_version(image.id)) {
        lu_log_err("Dangling handle.");
        return (xe_tex){.idx = -1, .layer = -1};
    }

    switch (img->asset.state) {
        case XE_ASSET_COMMITED:
            return img->tex;
        case XE_ASSET_STAGED:
            xe_render_tex_load(img->tex, img->data);
            stbi_image_free((void*)img->data);
            ((struct xe_asset_image*)img)->data = NULL;
            return img->tex;

        default:
            lu_log_err("Image in invalid state.");
            lu_err_assert(0);
            break;
    }
    return (xe_tex){.idx = -1, .layer = -1};
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
    struct xe_asset_image *img = NULL;
    for (int i = 0; i < XE_MAX_IMAGES; ++i) {
        if (g_assets.img[i].asset.state == XE_ASSET_FREE) {
            img = g_assets.img + i;
            img->asset.state = XE_ASSET_EMPTY;
            uint16_t ver = ++img->asset.version;
            hnd.id = xe_handle_gen(ver, i);
            break;
        }
    }

    return hnd;
}

static void
xe_image_generate_texture(struct xe_asset_image *img)
{
    lu_err_assert(img && img->data);
    lu_err_assert(img->asset.state == XE_ASSET_LOADING);
    img->tex = xe_render_tex_alloc((xe_texfmt){
        .width = img->w,
        .height = img->h,
        .format = xe_pixel_format_from_ch(img->c),
        .flags = 0  // Don't forward flags that prevent textures from grouping in arrays
    });
    lu_err_assert(img->tex.idx >= 0);
    img->asset.state = XE_ASSET_STAGED;
    xe_render_tex_load(img->tex, img->data);
    img->asset.state = XE_ASSET_COMMITED;
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
        struct xe_asset_image *img = (void*)xe_asset_image_data(hnd);
        img->path = "";
        img->data = pix_data;
        img->asset.state = XE_ASSET_LOADING;
        img->flags = tex_flags;
        img->w = w;
        img->h = h;
        img->c = c;
        xe_image_generate_texture(img);
        lu_err_assert(img->asset.state == XE_ASSET_COMMITED);
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
        xe_asset_image *img = (void*)xe_asset_image_data(hnd);
        img->asset.state = XE_ASSET_LOADING;
        int w, h, c;
        img->data = stbi_load(path, &w, &h, &c, 0);
        if (!img->data) {
            lu_log_err("Could not load image %s.", path);
            img->asset.state = XE_ASSET_FAILED;
            return hnd;
        }

        img->path = path;
        img->w = w;
        img->h = h;
        img->c = c;
        img->flags = tex_flags;
        xe_image_generate_texture(img);
        lu_err_assert(img->asset.state == XE_ASSET_COMMITED);
        stbi_image_free((stbi_uc*)img->data);
        img->data = NULL;
    }

    return hnd;
}


xe_pipeline
xe_asset_pipeline_load(const char *vert_path, const char *frag_path)
{
    xe_pipeline hnd = {.id = XE_MAX_PIPELINES};
    xe_asset_pipeline *pip = NULL;
    for (int i = 0; i < XE_MAX_PIPELINES; ++i) {
        if (g_assets.pipelines[i].asset.state == XE_ASSET_FREE) {
            pip = g_assets.pipelines + i;
            pip->asset.state = XE_ASSET_LOADING;
            uint16_t ver = ++pip->asset.version;
            hnd.id = xe_handle_gen(ver, i);
            break;
        }
    }

    if (hnd.id == XE_MAX_PIPELINES) {
        lu_log_err("Pipeline %s, %s could not be created. XE_MAX_PIPELINES reached.",
                    vert_path, frag_path);
        goto asset_failed;
    }

    enum { MAX_SOURCE_LEN = 4096 };
    char vert_buf[MAX_SOURCE_LEN];
    char frag_buf[MAX_SOURCE_LEN];

    size_t vert_len;
    bool ret = xe_file_read(vert_path, vert_buf, MAX_SOURCE_LEN, &vert_len);
    lu_err_assert(ret);
    if (!ret) {
        goto asset_failed;
    }

    size_t frag_len;
    ret = xe_file_read(frag_path, frag_buf, MAX_SOURCE_LEN, &frag_len);
    lu_err_assert(ret);
    if (!ret) {
        goto asset_failed;
    }

    xe_shader_sources src = {
        .vert_src = vert_buf,
        .vert_len = vert_len,
        .frag_src = frag_buf,
        .frag_len = frag_len
    };
    pip->id = xe_render_pipeline_alloc();
    if (!pip->id) {
        goto asset_failed;
    }

    bool compiled = xe_render_pipeline_compile(pip->id, src);
    if (!compiled) {
        goto asset_failed;
    }

    pip->asset.state = XE_ASSET_COMMITED;
    return hnd;

asset_failed:
    pip->asset.state = XE_ASSET_FAILED;
    return hnd;
}

const xe_asset_pipeline *
xe_asset_pipeline_data(xe_pipeline pipeline)
{
    int idx = xe_handle_index(pipeline.id);
    int ver = xe_handle_version(pipeline.id);
    return g_assets.pipelines[idx].asset.version == ver ? g_assets.pipelines + idx : NULL;
}

xe_program
xe_asset_pipeline_program(xe_pipeline pipeline)
{
    return xe_asset_pipeline_data(pipeline)->id;
}
