#include "yuvframe.h"
#include "defines.h"
#include <stdint.h>

int alloc_frame(Frame* f, int width, int height, int bit_depth, int chroma_format)
{
    unsigned char* yuv_buf = NULL;
    f->width[CIDX_Y] = width;
    f->height[CIDX_Y] = height;
    f->chroma_format = chroma_format;
    if (f->chroma_format == YUV420)
    {
        f->width[CIDX_U] = f->width[CIDX_V] = width / 2;
        f->height[CIDX_U] = f->height[CIDX_V] = height / 2;
    }
    else if (f->chroma_format == YUV422)
    {
        f->width[CIDX_U] = f->width[CIDX_V] = width / 2;
        f->height[CIDX_U] = f->height[CIDX_V] = height;
    }
    else if (f->chroma_format == YUV444)
    {
        f->width[CIDX_U] = f->width[CIDX_V] = width;
        f->height[CIDX_U] = f->height[CIDX_V] = height;
    }
    f->bit_depth = bit_depth;
    f->pixel_size = f->bit_depth == 8 ? 1 : 2;
    f->y_size = f->width[CIDX_Y] * f->height[CIDX_Y] * f->pixel_size;
    f->uv_size = f->width[CIDX_CHROMA] * f->height[CIDX_CHROMA] * f->pixel_size;
    f->frame_size = f->y_size + 2 * f->uv_size;
    yuv_buf = (unsigned char *)malloc(f->frame_size);
    f->yuv[CIDX_Y] = yuv_buf;
    f->yuv[CIDX_U] = yuv_buf + f->y_size;
    f->yuv[CIDX_V] = yuv_buf + f->y_size + f->uv_size;
    return 1;
}

void free_frame(Frame* f)
{
    free(f->yuv[CIDX_Y]);
}

int read_frame(FILE* in_f, Frame* f)
{
    if (f->y_size != fread(f->yuv[CIDX_Y], 1, f->y_size, in_f))
        return -1;
    if (f->uv_size != fread(f->yuv[CIDX_U], 1, f->uv_size, in_f))
        return -1;
    if (f->uv_size != fread(f->yuv[CIDX_V], 1, f->uv_size, in_f))
        return -1;
    return 0;
}

int read_nframe(FILE* in_f, Frame* f, int frm_num)
{
    int ret = 0;
#ifndef linux
    ret = _fseeki64(in_f, 0L, SEEK_SET);                      // seek to beginning
#else
    ret = fseeko64(in_f, 0L, SEEK_SET);                      // seek to beginning
#endif
    if (ret != 0)
        return -1;
#ifndef linux
    ret = _fseeki64(in_f, (int64_t)frm_num * f->frame_size, SEEK_SET); // seek to frame_number
#else
    ret = fseeko64(in_f, (int64_t)frm_num * f->frame_size, SEEK_SET); // seek to frame_number
#endif
    if (ret != 0)
        return -1;

    if (f->y_size != fread(f->yuv[CIDX_Y], 1, f->y_size, in_f))
        return -1;
    if (f->uv_size != fread(f->yuv[CIDX_U], 1, f->uv_size, in_f))
        return -1;
    if (f->uv_size != fread(f->yuv[CIDX_V], 1, f->uv_size, in_f))
        return -1;
    return 0;
}