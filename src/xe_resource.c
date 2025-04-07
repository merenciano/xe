#include "xe.h"
#include "xe_resource.h"
#include "xe_renderer.h"
#include "xe_platform.h"

#include <stb/stb_image.h>
#include <stdint.h>

struct xe_res {
    struct xe_res_image img[XE_MAX_IMAGES];

};

static struct xe_res g_res;

static inline const struct xe_res_image *xe_image_ptr(xe_image img);
const struct xe_res_image *
xe_image_ptr(xe_image img)
{
    uint16_t i = xe_res_index(img.id);
    return i < XE_MAX_IMAGES ? &g_res.img[i] : NULL;
}

xe_rend_tex
xe_image_tex(xe_image image)
{
    const struct xe_res_image *img = xe_image_ptr(image);
    xe_assert(img);
    if (img->res.version != xe_res_version(image.id)) {
        XE_LOG_ERR("Dangling handle.");
        return xe_rend_tex_invalid();
    }

    switch (img->res.state) {
        case XE_RS_COMMITED:
            return img->tex;
        case XE_RS_STAGED:
            xe_rend_tex_set(img->tex, img->data);
            return img->tex;

        default:
            XE_LOG_ERR("Image in invalid state.");
            xe_assert(0);
            break;
    }
    return xe_rend_tex_invalid();
}

static inline int
xe_pixel_format_from_ch(int ch)
{
    static const int formats[] = { XE_ERR, XE_TEX_R, XE_TEX_RG, XE_TEX_RGB, XE_TEX_RGBA };
    xe_assert((ch > 0) && (ch < (sizeof(formats) / sizeof(*formats))) && "Out of range");
    return formats[ch];
}

xe_image
xe_image_load(const char *path, int tex_flags)
{
    xe_image h = {.id = XE_MAX_IMAGES};
    struct xe_res_image *img = NULL;
    for (int i = 0; i < XE_MAX_IMAGES; ++i) {
        if (g_res.img[i].res.state == XE_RS_FREE) {
            img = g_res.img + i;
            img->res.state = XE_RS_EMPTY;
            uint16_t ver = img->res.version++;
            h.id = xe_res_handle_gen(ver, i);
            break;
        }
    }

    if (img) {
        img->path = path;
        img->res.state = XE_RS_LOADING;
        int w, h, c;
        img->data = stbi_load(img->path, &w, &h, &c, 0);
        if (!img->data) {
            XE_LOG_ERR("Could not load image %s.", img->path);
            img->res.state = XE_RS_FAILED;
        } else {
            img->flags = tex_flags;
            img->w = w;
            img->h = h;
            img->c = c;
            img->tex = xe_rend_tex_alloc((xe_rend_texfmt){
                .width = img->w,
                .height = img->h,
                .format = xe_pixel_format_from_ch(img->c),
                .flags = 0  // Don't forward flags that prevent textures from grouping in arrays
            });
            assert(img->tex.idx >= 0);
            img->res.state = XE_RS_STAGED;

            xe_rend_tex_set(img->tex, img->data);
            img->res.state = XE_RS_COMMITED;
        }
    }

    return h;
}
