#include "quality_metric.h"
#include <math.h>

void get_default_qmctx(QMContext* qmctx)
{
    sprintf(qmctx->s_ref_fname, "");
    sprintf(qmctx->s_dst_fname, "");
    sprintf(qmctx->s_out_fname, "");
    qmctx->i_bit_depth       = 8;
    qmctx->i_frame_num       = 999999;
    qmctx->i_chroma_format   = YUV420;
    qmctx->ia_width[CIDX_Y]  = 1920;
    qmctx->ia_width[CIDX_U]  = qmctx->ia_width[CIDX_V] = 960;
    qmctx->ia_height[CIDX_Y] = 1080;
    qmctx->ia_height[CIDX_U] = qmctx->ia_height[CIDX_V] = 540;
    qmctx->i_metric_method   = (M_PSNR | M_SSIM);
    qmctx->i_ref_skip_num    = qmctx->i_dst_skip_num    = 0;
    qmctx->i_auto_skip       = 0;
    qmctx->i_threads         = 1;
    qmctx->i_metric_method   = M_PSNR;
}

int64_t get_block_ssd_8bit(unsigned char* pix1, unsigned char* pix2, int width, int height)
{
    int64_t sum = 0, ssd;
    int x, y, tmp;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            tmp = pix1[x] - pix2[x];
            sum += (tmp * tmp);
        }
        pix1 += width;
        pix2 += width;
    }
    ssd = sum;
    return ssd;
}

int64_t get_block_ssd_10bit(uint16_t* pix1, uint16_t* pix2, int width, int height)
{
    int64_t sum = 0, ssd;
    int x, y, tmp;
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            tmp = pix1[x] - pix2[x];
            sum += (tmp * tmp);
        }
        pix1 += width;
        pix2 += width;
    }
    ssd = sum;
    return ssd;
}

void get_frame_ssd(Frame* ref, Frame* dst, int64_t ssd[])
{
    if (ref->pixel_size == 1) // 8-bit
    {
        ssd[CIDX_Y] = get_block_ssd_8bit(ref->yuv[CIDX_Y], dst->yuv[CIDX_Y], ref->width[CIDX_Y], ref->height[CIDX_Y]);
        ssd[CIDX_U] = get_block_ssd_8bit(ref->yuv[CIDX_U], dst->yuv[CIDX_U], ref->width[CIDX_U], ref->height[CIDX_U]);
        ssd[CIDX_V] = get_block_ssd_8bit(ref->yuv[CIDX_V], dst->yuv[CIDX_V], ref->width[CIDX_V], ref->height[CIDX_V]);
    }
    else if (ref->pixel_size == 2) // 10-bit
    {
        ssd[CIDX_Y] = get_block_ssd_10bit((uint16_t*)ref->yuv[CIDX_Y], (uint16_t*)dst->yuv[CIDX_Y], ref->width[CIDX_Y], ref->height[CIDX_Y]);
        ssd[CIDX_U] = get_block_ssd_10bit((uint16_t*)ref->yuv[CIDX_U], (uint16_t*)dst->yuv[CIDX_U], ref->width[CIDX_U], ref->height[CIDX_U]);
        ssd[CIDX_V] = get_block_ssd_10bit((uint16_t*)ref->yuv[CIDX_V], (uint16_t*)dst->yuv[CIDX_V], ref->width[CIDX_V], ref->height[CIDX_V]);
    }
}

int jump_to_frame(FILE* in_f, int64_t frame_size, int64_t frame_number)
{
    int ret = 0;
    ret = _fseeki64(in_f, 0L, SEEK_SET);                         // seek to beginning
    if (ret != 0)
        return -1;

#if 0
    long init_size = ftell(in_f);
    ret = _fseeki64(in_f, 0L, SEEK_END);
    int64_t fsize = _ftelli64(in_f);
    long fnum = fsize / frame_size;
    ret = _fseeki64(in_f, 0L, SEEK_SET);
#endif

    ret = _fseeki64(in_f, frame_number * frame_size, SEEK_SET); // seek to frame_number
    if (ret != 0)
        return -1;
    return 0;
}

#define MIN(a, b) ((a) < (b) ? (a) : (b))
double ssd_to_psnr(double max_ssd, int64_t act_ssd)
{
    double max_psnr = 99.9999;
    if (act_ssd > 0)
    {
        double psnr = 10.0 * log10(max_ssd / act_ssd);
        return MIN(psnr, max_psnr);
    }
    else
        return max_psnr;
}