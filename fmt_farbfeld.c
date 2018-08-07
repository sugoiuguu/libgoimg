#include <stdlib.h>
#include <string.h>

#include "color.h"
#include "fmt_farbfeld.h"
#include "util.h"

struct _s_bufwriter {
    void *buf;
    int avail;
};

static int _s_bufwrite(void *dst, char *buf, int size)
{
    struct _s_bufwriter *s = (struct _s_bufwriter *)dst;

    if (unlikely(!s->avail))
        return 0;

    if (unlikely(size > s->avail))
        size = s->avail;

    memcpy(s->buf, buf, size);
    s->avail -= size;
    s->buf += size;

    return size;
}

int im_farbfeld_dec(Image_t *img, rfun_t rf, void *src)
{
    char magic[16];

    /* read the magic */
    if (unlikely(rf(src, magic, 16) < 0))
        return -1;

    /* read dims */
    img->w = ntohl(*(uint32_t *)(magic + 8));
    img->h = ntohl(*(uint32_t *)(magic + 12));

    img->size = img->w * img->h * sizeof(uint64_t);
    img->img = _xalloc(img->alloc, img->size);
    img->color_model = im_colormodel_nrgba64;
    img->at = im_nrgba64_at;
    img->set = im_nrgba64_set;

    struct _s_bufwriter s = {img->img, img->size};

    return (unlikely(rwcpy(_s_bufwrite, &s, rf, src) < 0)) ? -1 : 0;
}

int im_farbfeld_enc(Image_t *img, wfun_t wf, void *dst)
{
    /* write the magic */
    if (unlikely(wf(dst, "farbfeld", 8) < 0))
        return -1;

    /* write dimensions */
    uint32_t dim;

    dim = htonl((uint32_t)img->w);
    if (unlikely(wf(dst, (char *)&dim, sizeof(uint32_t)) < 0))
        return -1;

    dim = htonl((uint32_t)img->h);
    if (unlikely(wf(dst, (char *)&dim, sizeof(uint32_t)) < 0))
        return -1;

    /*
     * write the pixel data
     * */

    if (likely(img->color_model == im_colormodel_nrgba64))
        return (unlikely(wf(dst, (char *)img->img, img->size) < 0)) ? -1 : 0;

    /* lossy */
    int x, y, err = 0;
    Color_t c_src = im_newcolor_from_img(img),
            c_dst = im_newcolor_nrgba64();

    for (y = 0; y < img->h; y++) {
        for (x = 0; x < img->w; x++) {
            img->at(img, x, y, &c_src);
            im_colormodel_nrgba64(&c_dst, &c_src);

            if (unlikely(wf(dst, (char *)c_dst.color, sizeof(uint64_t)) < 0)) {
                err = -1;
                goto done;
            }
        }
    }

done:
    free(c_src.color);
    free(c_dst.color);

    return err;
}
