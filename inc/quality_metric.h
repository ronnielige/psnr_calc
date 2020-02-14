#ifndef _QUALITY_METRIC_H_
#define _QUALITY_METRIC_H_
#include "defines.h"
#include "yuvframe.h"
#include "w32thread.h"

typedef struct _stat_result
{
    double avg_psnr[3];
    double avg_ssim[3];
    int    i_do_frames;
    pthread_mutex_t mtx;
}StatResult;

typedef struct _QualityMetric_Context
{
#define FILE_NAME_LENGTH 512
    char  s_ref_fname[FILE_NAME_LENGTH];  // reference yuv file name
    char  s_dst_fname[FILE_NAME_LENGTH];  // dist yuv file name
    char  s_out_fname[FILE_NAME_LENGTH];  // output result file name
    int   ia_width[3];
    int   ia_height[3];
    int   i_chroma_format;
    int   i_bit_depth;
    int   i_frame_num;
    int   i_ref_skip_num;
    int   i_dst_skip_num;
    int   i_auto_skip;                    // auto decide skipped frame numbers of ref and dst yuv. default 0
    int   i_metric_method;                // quality metric method(psnr & ssim): 1 - psnr, 2 - ssim, 3 - psnr + ssim
    int   i_threads;
    int   i_exit;
    StatResult result_stat;
}QualityMetricContext, QMContext;

int64_t get_block_ssd_8bit(unsigned char* pix1, unsigned char* pix2, int width, int height);
int64_t get_block_ssd_10bit(uint16_t* pix1, uint16_t* pix2, int width, int height);
void    get_frame_ssd(Frame* ref, Frame* dst, int64_t ssd[]);
int     jump_to_frame(FILE* in_f, int64_t frame_size, int64_t frame_number);
double  ssd_to_psnr(double max_ssd, int64_t act_ssd);
void    get_default_qmctx(QMContext* qmctx);
float   ssim_plane(uint8_t *main, int main_stride,
                   uint8_t *ref, int ref_stride,
                   int width, int height, void *temp, int max);
float ssim_plane_16bit(uint8_t *main, int main_stride,
                       uint8_t *ref, int ref_stride,
                       int width, int height, void *temp,
                       int max);

#endif