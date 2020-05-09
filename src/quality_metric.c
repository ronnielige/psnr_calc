#include "quality_metric.h"
#include <math.h>
#include <string.h>
#ifdef linux
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#endif
#include <stdio.h>

void get_default_qmctx(QMContext* qmctx)
{
    sprintf(qmctx->s_ref_fname, "");
    sprintf(qmctx->s_dst_fname, "");
    sprintf(qmctx->s_out_fname, "");
    qmctx->i_bit_depth       = 8;
    qmctx->i_frame_num       = 99999;
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
    qmctx->i_exit            = 0;
    qmctx->out_file          = stdout;
    memset(&qmctx->result_stat, 0, sizeof(StatResult));
    pthread_mutex_init(&qmctx->result_stat.mtx, NULL);
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
#ifndef linux
    ret = _fseeki64(in_f, 0L, SEEK_SET);                         // seek to beginning
#else
    ret = fseeko64(in_f, 0L, SEEK_SET);                         // seek to beginning
#endif
    if (ret != 0)
        return -1;

#if 0
    long init_size = ftell(in_f);
    ret = _fseeki64(in_f, 0L, SEEK_END);
    int64_t fsize = _ftelli64(in_f);
    long fnum = fsize / frame_size;
    ret = _fseeki64(in_f, 0L, SEEK_SET);
#endif

#ifndef linux
    ret = _fseeki64(in_f, (int64_t)frame_number * frame_size, SEEK_SET); // seek to frame_number
#else
    ret = fseeko64(in_f, (int64_t)frame_number * frame_size, SEEK_SET); // seek to frame_number
#endif
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

/* Following functions refer to ffmpeg */
static void ssim_4x4xn(const uint8_t *main, ptrdiff_t main_stride,
                       const uint8_t *ref, ptrdiff_t ref_stride,
                       int(*sums)[4],      int width)
{
    int x, y, z;

    for (z = 0; z < width; z++) {
        uint32_t s1 = 0, s2 = 0, ss = 0, s12 = 0;

        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                int a = main[x + y * main_stride];
                int b = ref[x + y * ref_stride];

                s1 += a;
                s2 += b;
                ss += a*a;
                ss += b*b;
                s12 += a*b;
            }
        }

        sums[z][0] = s1;
        sums[z][1] = s2;
        sums[z][2] = ss;
        sums[z][3] = s12;
        main += 4;
        ref += 4;
    }
}

static float ssim_end1(int s1, int s2, int ss, int s12)
{
    static const int ssim_c1 = (int)(.01*.01 * 255 * 255 * 64 + .5);
    static const int ssim_c2 = (int)(.03*.03 * 255 * 255 * 64 * 63 + .5);

    int fs1 = s1;
    int fs2 = s2;
    int fss = ss;
    int fs12 = s12;
    int vars = fss * 64 - fs1 * fs1 - fs2 * fs2;
    int covar = fs12 * 64 - fs1 * fs2;

    return (float)(2 * fs1 * fs2 + ssim_c1) * (float)(2 * covar + ssim_c2)
        / ((float)(fs1 * fs1 + fs2 * fs2 + ssim_c1) * (float)(vars + ssim_c2));
}

static float ssim_endn(const int(*sum0)[4], const int(*sum1)[4], int width)
{
    float ssim = 0.0;
    int i;

    for (i = 0; i < width; i++)
        ssim += ssim_end1(sum0[i][0] + sum0[i + 1][0] + sum1[i][0] + sum1[i + 1][0],
            sum0[i][1] + sum0[i + 1][1] + sum1[i][1] + sum1[i + 1][1],
            sum0[i][2] + sum0[i + 1][2] + sum1[i][2] + sum1[i + 1][2],
            sum0[i][3] + sum0[i + 1][3] + sum1[i][3] + sum1[i + 1][3]);
    return ssim;
}

#define FFSWAP(type,a,b) do{type SWAP_tmp= b; b= a; a= SWAP_tmp;}while(0)

static void ssim_4x4xn_16bit(const uint8_t *main8, ptrdiff_t main_stride,
                             const uint8_t *ref8, ptrdiff_t ref_stride,
                             int64_t(*sums)[4], int width)
{
    const uint16_t *main16 = (const uint16_t *)main8;
    const uint16_t *ref16 = (const uint16_t *)ref8;
    int x, y, z;

    main_stride >>= 1;
    ref_stride >>= 1;

    for (z = 0; z < width; z++) {
        uint64_t s1 = 0, s2 = 0, ss = 0, s12 = 0;

        for (y = 0; y < 4; y++) {
            for (x = 0; x < 4; x++) {
                unsigned a = main16[x + y * main_stride];
                unsigned b = ref16[x + y * ref_stride];

                s1 += a;
                s2 += b;
                ss += a*a;
                ss += b*b;
                s12 += a*b;
            }
        }

        sums[z][0] = s1;
        sums[z][1] = s2;
        sums[z][2] = ss;
        sums[z][3] = s12;
        main16 += 4;
        ref16 += 4;
    }
}

static float ssim_end1x(int64_t s1, int64_t s2, int64_t ss, int64_t s12, int max)
{
    int64_t ssim_c1 = (int64_t)(.01*.01*max*max * 64 + .5);
    int64_t ssim_c2 = (int64_t)(.03*.03*max*max * 64 * 63 + .5);

    int64_t fs1 = s1;
    int64_t fs2 = s2;
    int64_t fss = ss;
    int64_t fs12 = s12;
    int64_t vars = fss * 64 - fs1 * fs1 - fs2 * fs2;
    int64_t covar = fs12 * 64 - fs1 * fs2;

    return (float)(2 * fs1 * fs2 + ssim_c1) * (float)(2 * covar + ssim_c2)
        / ((float)(fs1 * fs1 + fs2 * fs2 + ssim_c1) * (float)(vars + ssim_c2));
}

static float ssim_endn_16bit(const int64_t(*sum0)[4], const int64_t(*sum1)[4], int width, int max)
{
    float ssim = 0.0;
    int i;

    for (i = 0; i < width; i++)
        ssim += ssim_end1x(sum0[i][0] + sum0[i + 1][0] + sum1[i][0] + sum1[i + 1][0],
            sum0[i][1] + sum0[i + 1][1] + sum1[i][1] + sum1[i + 1][1],
            sum0[i][2] + sum0[i + 1][2] + sum1[i][2] + sum1[i + 1][2],
            sum0[i][3] + sum0[i + 1][3] + sum1[i][3] + sum1[i + 1][3],
            max);
    return ssim;
}

/* main_stride ref_stride is stride in bytes.*/
float ssim_plane(uint8_t *main, int main_stride,
                 uint8_t *ref,  int ref_stride,
                 int width, int height, void *temp, int max)
{
    int z = 0, y;
    float ssim = 0.0;
    int(*sum0)[4] = temp;
    int(*sum1)[4] = sum0 + (width >> 2) + 3;

    width >>= 2;
    height >>= 2;

    for (y = 1; y < height; y++) 
    {
        for (; z <= y; z++) 
        {
            FFSWAP(void*, sum0, sum1);
            ssim_4x4xn(&main[4 * z * main_stride], main_stride,
                       &ref[4 * z * ref_stride],   ref_stride,
                       sum0, width);
        }

        ssim += ssim_endn((const int(*)[4])sum0, (const int(*)[4])sum1, width - 1);
    }

    return ssim / ((height - 1) * (width - 1));
}

float ssim_plane_16bit(uint8_t *main, int main_stride,
                       uint8_t *ref,  int ref_stride,
                       int width, int height, void *temp,
                       int max)
{
    int z = 0, y;
    float ssim = 0.0;
    int64_t(*sum0)[4] = temp;
    int64_t(*sum1)[4] = sum0 + (width >> 2) + 3;

    width >>= 2;
    height >>= 2;

    for (y = 1; y < height; y++) {
        for (; z <= y; z++) {
            FFSWAP(void*, sum0, sum1);
            ssim_4x4xn_16bit(&main[4 * z * main_stride], main_stride,
                             &ref[4 * z * ref_stride],   ref_stride,
                             sum0, width);
        }

        ssim += ssim_endn_16bit((const int64_t(*)[4])sum0, (const int64_t(*)[4])sum1, width - 1, max);
    }

    return ssim / ((height - 1) * (width - 1));
}