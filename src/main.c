#include "quality_metric.h"
#include "getopt.h"
#include "options.h"
#include <stdlib.h>
#include <stdio.h>
#include "threadpool.h"
#include <string.h>
#ifdef linux
#include <unistd.h>
#endif
#include "version.h"

const char* cf_name[4] = { "YUV400", "YUV420", "YUV422", "YUV444" };

enum 
{
    TH_NOTUSED,
    TH_USED,
};

typedef struct _threadCtx
{
    FILE*      ref_file;
    FILE*      dst_file;
    Frame      ref_frame;
    Frame      dst_frame;
    double     frame_psnr[3];
    double     frame_ssim[3];
    int*       temp;
    QMContext* qmctx;
    int        i_proc_frm_num;
    volatile int i_status;
    pthread_mutex_t mtx;
    threadpool_t*   p_pool;
}threadCtx;

void show_parameters(QMContext* qmctx)
{
    FILE* out_file = qmctx->out_file;
    fprintf(out_file, "ref yuv:            %s\n", qmctx->s_ref_fname);
    fprintf(out_file, "dst yuv:            %s\n", qmctx->s_dst_fname);
    fprintf(out_file, "width     / height       / bit_depth    / chroma_format :  %5d / %5d / %5d / %5s\n", 
           qmctx->ia_width[CIDX_Y], qmctx->ia_height[CIDX_Y], qmctx->i_bit_depth, cf_name[qmctx->i_chroma_format]);
    fprintf(out_file, "frame_num / ref_skip_num / dst_skip_num / auto_skip     :  %5d / %5d / %5d / %5d\n", 
           qmctx->i_frame_num, qmctx->i_ref_skip_num, qmctx->i_dst_skip_num, qmctx->i_auto_skip);
    fprintf(out_file, "threads   / metric_method/ version                      :  %5d / %5d / %d.%d.%d.%d\n\n", 
           qmctx->i_threads, qmctx->i_metric_method, VER_MAJOR, VER_MINOR, VER_RELEASE, VER_BUILD);
}

int parse_cmds(int argc, char**argv, QMContext* qmctx)
{
    if (argc == 1)
    {
        showHelp();
        return -1;
    }

    for (optind = 0;;) // presets
    {
        int long_options_index = -1;
        int c = getopt_long(argc, argv, short_options, long_options, &long_options_index);
        if (c == -1)
            break;

        switch (c)
        {
        case 'h':
            showHelp();
            return -1;
        default:
            if (long_options_index < 0 && c > 0)
            {
                for (size_t i = 0; i < sizeof(long_options) / sizeof(long_options[0]); i++)
                {
                    if (long_options[i].val == c)
                    {
                        long_options_index = (int)i;
                        break;
                    }
                }

                if (long_options_index < 0)
                {
                    /* getopt_long might have already printed an error message */
                    if (c != 63)
                        printf("internal error: short option '%c' has no long option\n", c);
                    return 1;
                }
            }
            if (long_options_index < 0)
            {
                printf("short option '%c' unrecognized\n", c);
                return 1;
            }
#define OPT(longname) \
    else if (!strcmp(long_options[long_options_index].name, longname))
#define OPT2(name1, name2) \
    else if (!strcmp(long_options[long_options_index].name, name1) || \
             !strcmp(long_options[long_options_index].name, name2))  

            if (0);
            OPT("ref")                   sprintf(qmctx->s_ref_fname, "%s", optarg);
            OPT("dst")                   sprintf(qmctx->s_dst_fname, "%s", optarg);
            OPT("output")                sprintf(qmctx->s_out_fname, "%s", optarg);
            OPT("bitdepth")              qmctx->i_bit_depth = atoi(optarg);
            OPT("width")                 qmctx->ia_width[CIDX_Y] = atoi(optarg);
            OPT("height")                qmctx->ia_height[CIDX_Y] = atoi(optarg);
            OPT("frames")                qmctx->i_frame_num = atoi(optarg);
            OPT("chroma-format")         qmctx->i_chroma_format = atoi(optarg);
            OPT("threads")               qmctx->i_threads = atoi(optarg);
            OPT("ref-skip-num")          qmctx->i_ref_skip_num = atoi(optarg);
            OPT("dst-skip-num")          qmctx->i_dst_skip_num = atoi(optarg);
            OPT("auto-skip")             qmctx->i_auto_skip = atoi(optarg);
            OPT("threads")               qmctx->i_threads = atoi(optarg);
            OPT("metric-method")         qmctx->i_metric_method = atoi(optarg);
        }
    }
    return 0;
}


int process_quality_metric_singlethread(QMContext* qmctx)
{
    FILE* ref_file, *dst_file, *out_file = qmctx->out_file;
    Frame ref_frame, dst_frame;
    int64_t frame_ssd[3];
    double  frame_psnr[3], frame_ssim[3];
    double  avg_psnr[3] = { 0.0, 0.0, 0.0 }, avg_ssim[3] = { 0.0, 0.0, 0.0 };
    double  pixel_max_ssd = 0;
    double  progress = 0;
    int*    temp;
    int     size_temp;
    int     srcfile_total_frms = 0, dstfile_total_frms = 0, max_avail_frames = 0;
    int i;

    ref_file = fopen(qmctx->s_ref_fname, "rb");
    if (NULL == ref_file)
    {
        fprintf(stderr, "Open ref yuv file %s error!\n", qmctx->s_ref_fname);
        return -1;
    }
    dst_file = fopen(qmctx->s_dst_fname, "rb");
    if (NULL == ref_file)
    {
        fprintf(stderr, "Open dst yuv file %s error!\n", qmctx->s_dst_fname);
        return -1;
    }

    alloc_frame(&ref_frame, qmctx->ia_width[CIDX_Y], qmctx->ia_height[CIDX_Y], qmctx->i_bit_depth, qmctx->i_chroma_format);
    alloc_frame(&dst_frame, qmctx->ia_width[CIDX_Y], qmctx->ia_height[CIDX_Y], qmctx->i_bit_depth, qmctx->i_chroma_format);

    srcfile_total_frms = get_file_frame_num(ref_file, &ref_frame);
    dstfile_total_frms = get_file_frame_num(dst_file, &dst_frame);
    max_avail_frames   = srcfile_total_frms < dstfile_total_frms ? dstfile_total_frms : srcfile_total_frms;
    max_avail_frames   = max_avail_frames < qmctx->i_frame_num ? max_avail_frames : qmctx->i_frame_num;
    fprintf(out_file, "Reference file contain %d frames, Dst file contain %d frames!\n", srcfile_total_frms, dstfile_total_frms);

    if (jump_to_frame(ref_file, ref_frame.frame_size, qmctx->i_ref_skip_num) < 0)
    {
        fprintf(stderr, "Ref yuv jump to %d frame failed!\n", qmctx->i_ref_skip_num);
        return 0;
    }
    if (jump_to_frame(dst_file, dst_frame.frame_size, qmctx->i_dst_skip_num) < 0)
    {
        fprintf(stderr, "Dst yuv jump to %d frame failed!\n", qmctx->i_dst_skip_num);
        return 0;
    }

    int pixel_max_value = (1 << qmctx->i_bit_depth) - 1;
    int pixel_byte = (qmctx->i_bit_depth > 8 ? 2 : 1);
    pixel_max_ssd = pixel_max_value * pixel_max_value;

    //// Step 1: Show Title
    fprintf(out_file, " Frame    ");
    if(qmctx->i_metric_method & M_PSNR)
        fprintf(out_file, "PSNR_Y    PSNR_U    PSNR_V    ");
    if (qmctx->i_metric_method & M_SSIM)
        fprintf(out_file, "SSIM_Y    SSIM_U    SSIM_V    ");
    fprintf(out_file, "\n");

    //// Step 2: Metric Quality
    size_temp = (2 * qmctx->ia_width[CIDX_Y] + 12) * (qmctx->i_bit_depth > 8 ? sizeof(int64_t[4]) : sizeof(int[4]));
    temp = (int *)malloc(size_temp);
    memset(temp, 0, size_temp);

    fprintf(stderr, "Finished %3d%%", (int)0);
    for (i = 0; i < qmctx->i_frame_num; i++)
    {
        if (read_nframe(ref_file, &ref_frame, i) < 0)
            break;
        if (read_nframe(dst_file, &dst_frame, i) < 0)
            break;
        fprintf(out_file, "%6d    ", i + 1);
        if (qmctx->i_metric_method & M_PSNR)
        {
            get_frame_ssd(&ref_frame, &dst_frame, frame_ssd);
            for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
            {
                frame_psnr[cidx] = ssd_to_psnr(pixel_max_ssd * ref_frame.width[cidx] * ref_frame.height[cidx], frame_ssd[cidx]);
                avg_psnr[cidx] += frame_psnr[cidx];
            }
            fprintf(out_file, "%6.3f    %6.3f    %6.3f    ", frame_psnr[CIDX_Y], frame_psnr[CIDX_U], frame_psnr[CIDX_V]);
        }
        if (qmctx->i_metric_method & M_SSIM)
        {
            for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
            {
                if(qmctx->i_bit_depth == 8)
                    frame_ssim[cidx] = ssim_plane(ref_frame.yuv[cidx], ref_frame.width[cidx],
                                                  dst_frame.yuv[cidx], dst_frame.width[cidx],
                                                  ref_frame.width[cidx], ref_frame.height[cidx], temp, pixel_max_value);
                else if (qmctx->i_bit_depth == 10)
                    frame_ssim[cidx] = ssim_plane_16bit(ref_frame.yuv[cidx], ref_frame.width[cidx] * pixel_byte,
                                                        dst_frame.yuv[cidx], dst_frame.width[cidx] * pixel_byte,
                                                        ref_frame.width[cidx], ref_frame.height[cidx], temp, pixel_max_value);
                avg_ssim[cidx] += frame_ssim[cidx];
            }
            fprintf(out_file, "%6.3f    %6.3f    %6.3f    ", frame_ssim[CIDX_Y], frame_ssim[CIDX_U], frame_ssim[CIDX_V]);
        }
        fprintf(out_file, "\n");
        fflush(out_file);
        progress = 100 * (double)i / max_avail_frames;
        fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\bFinished %3d%%", (int)progress);
        fflush(stderr);
    }
    fprintf(stderr, "\b\b\b\b\b\b\b\b\b\b\b\b\bFinished %3d%%\n", (int)100);

    /// Step 3. Show Average result
    qmctx->i_frame_num = i == 0 ? 1 : i;
    fprintf(out_file, "\nAverage   ");
    if (qmctx->i_metric_method & M_PSNR)
        fprintf(out_file, "%6.3f    %6.3f    %6.3f    ", avg_psnr[CIDX_Y] / qmctx->i_frame_num, avg_psnr[CIDX_U] / qmctx->i_frame_num, avg_psnr[CIDX_V] / qmctx->i_frame_num);
    if (qmctx->i_metric_method & M_SSIM)
        fprintf(out_file, "%6.3f    %6.3f    %6.3f    ", avg_ssim[CIDX_Y] / qmctx->i_frame_num, avg_ssim[CIDX_U] / qmctx->i_frame_num, avg_ssim[CIDX_V] / qmctx->i_frame_num);
    fprintf(out_file, "\n");

    /// Step 4. Release resource
    free_frame(&ref_frame);
    free_frame(&dst_frame);
    free(temp);
    fclose(dst_file);
    fclose(ref_file);
    return 0;
}

int init_thread_context(threadCtx* tctx, QMContext* qmctx, threadpool_t* p_pool)
{
    tctx->p_pool   = p_pool;
    tctx->qmctx    = qmctx;
    tctx->ref_file = fopen(qmctx->s_ref_fname, "rb");
    if (NULL == tctx->ref_file)
    {
        printf("Open ref yuv file %s error!\n", qmctx->s_ref_fname);
        return -1;
    }
    tctx->dst_file = fopen(qmctx->s_dst_fname, "rb");
    if (NULL == tctx->ref_file)
    {
        printf("Open dst yuv file %s error!\n", qmctx->s_dst_fname);
        return -1;
    }
    alloc_frame(&tctx->ref_frame, qmctx->ia_width[CIDX_Y], qmctx->ia_height[CIDX_Y], qmctx->i_bit_depth, qmctx->i_chroma_format);
    alloc_frame(&tctx->dst_frame, qmctx->ia_width[CIDX_Y], qmctx->ia_height[CIDX_Y], qmctx->i_bit_depth, qmctx->i_chroma_format);

    int size_temp = (2 * qmctx->ia_width[CIDX_Y] + 12) * (qmctx->i_bit_depth > 8 ? sizeof(int64_t[4]) : sizeof(int[4]));
    tctx->temp = (int *)malloc(size_temp);
    memset(tctx->temp, 0, size_temp);

    tctx->i_status = TH_NOTUSED;
    pthread_mutex_init(&tctx->mtx, NULL);
    return 0;
}

threadCtx* get_one_thread_context(threadCtx* tctx_array, int len)
{
    while (1)
    {
        for (int i = 0; i < len; i++)
        {
            if (tctx_array[i].i_status == TH_NOTUSED)
            {
                tctx_array[i].i_status = TH_USED;
                return &tctx_array[i];
            }
        } 
#ifndef linux
        Sleep(1);
#else
        usleep(1000);
#endif
    }
}

void release_one_thread_context(threadCtx* tctx)
{
    tctx->i_status = TH_NOTUSED;
}

void* process_one_frame(void* arg)
{
    threadCtx*      tctx = (threadCtx *)arg;
    QMContext*     qmctx = tctx->qmctx;
    int  pixel_max_value = (1 << qmctx->i_bit_depth) - 1;
    int  pixel_byte      = (qmctx->i_bit_depth > 8 ? 2 : 1);
    double pixel_max_ssd = pixel_max_value * pixel_max_value;
    int64_t frame_ssd[3];
    char output_str[500];

    if (qmctx->i_exit == 1)
        return NULL;

    if (read_nframe(tctx->ref_file, &tctx->ref_frame, qmctx->i_ref_skip_num + tctx->i_proc_frm_num) < 0)
    {
        qmctx->i_exit = 1;
        return NULL;
    }
    if (read_nframe(tctx->dst_file, &tctx->dst_frame, qmctx->i_dst_skip_num + tctx->i_proc_frm_num) < 0)
    {
        qmctx->i_exit = 1;
        return NULL;
    }

    if (qmctx->i_metric_method & M_PSNR)
    {
        get_frame_ssd(&tctx->ref_frame, &tctx->dst_frame, frame_ssd);
        for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
        {
            tctx->frame_psnr[cidx] = ssd_to_psnr(pixel_max_ssd * tctx->ref_frame.width[cidx] * tctx->ref_frame.height[cidx], frame_ssd[cidx]);
            //avg_psnr[cidx] += frame_psnr[cidx];
        }
        sprintf(output_str, "Frame %5d: %6.3f    %6.3f    %6.3f    ", tctx->i_proc_frm_num + 1, tctx->frame_psnr[CIDX_Y], tctx->frame_psnr[CIDX_U], tctx->frame_psnr[CIDX_V]);
    }
    if (qmctx->i_metric_method & M_SSIM)
    {
        for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
        {
            if (qmctx->i_bit_depth == 8)
                tctx->frame_ssim[cidx] = ssim_plane(tctx->ref_frame.yuv[cidx], tctx->ref_frame.width[cidx],
                                                    tctx->dst_frame.yuv[cidx], tctx->dst_frame.width[cidx],
                                                    tctx->ref_frame.width[cidx], tctx->ref_frame.height[cidx], tctx->temp, pixel_max_value);
            else if (qmctx->i_bit_depth == 10)
                tctx->frame_ssim[cidx] = ssim_plane_16bit(tctx->ref_frame.yuv[cidx], tctx->ref_frame.width[cidx] * pixel_byte,
                                                          tctx->dst_frame.yuv[cidx], tctx->dst_frame.width[cidx] * pixel_byte,
                                                          tctx->ref_frame.width[cidx], tctx->ref_frame.height[cidx], tctx->temp, pixel_max_value);
            //avg_ssim[cidx] += frame_ssim[cidx];
        }
        sprintf(output_str + strlen(output_str), "%6.3f    %6.3f    %6.3f    ", tctx->frame_ssim[CIDX_Y], tctx->frame_ssim[CIDX_U], tctx->frame_ssim[CIDX_V]);
    }

    pthread_mutex_lock(&qmctx->result_stat.mtx);
    for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
    {
        qmctx->result_stat.avg_psnr[cidx] += tctx->frame_psnr[cidx];
        qmctx->result_stat.avg_ssim[cidx] += tctx->frame_ssim[cidx];
    }
    qmctx->result_stat.i_do_frames++;
    pthread_mutex_unlock(&qmctx->result_stat.mtx);

    release_one_thread_context(tctx);
    printf("%s\n", output_str);
    return NULL;
}

void process_quality_metric_multithread(QMContext* qmctx)
{
    int i_threads  = qmctx->i_threads;
    int i_tctx_len = i_threads + 4;
    threadCtx* tctx_arr = (threadCtx *)malloc(i_tctx_len * sizeof(threadCtx));
    threadCtx* tctx = NULL;
    threadpool_t* threadp;
    threadpool_init(&threadp, i_threads);
    for (int i = 0; i < i_tctx_len; i++)
        init_thread_context(tctx_arr + i, qmctx, threadp);

    for (int i = 0; i < qmctx->i_frame_num; i++)
    {
        if (qmctx->i_exit == 0)
        {
            tctx = get_one_thread_context(tctx_arr, i_tctx_len);
            tctx->i_proc_frm_num = i;
            threadpool_run(threadp, process_one_frame, tctx, 0);
        }
    }
    threadpool_delete(threadp);

    StatResult* res = &qmctx->result_stat;
    res->i_do_frames == 0 ? 1 : res->i_do_frames;
    printf("\nAverage   ");
    if (qmctx->i_metric_method & M_PSNR)
        printf("%6.3f    %6.3f    %6.3f    ", res->avg_psnr[CIDX_Y] / res->i_do_frames, res->avg_psnr[CIDX_U] / res->i_do_frames, res->avg_psnr[CIDX_V] / res->i_do_frames);
    if (qmctx->i_metric_method & M_SSIM)
        printf("%6.3f    %6.3f    %6.3f    ", res->avg_ssim[CIDX_Y] / res->i_do_frames, res->avg_ssim[CIDX_U] / res->i_do_frames, res->avg_ssim[CIDX_V] / res->i_do_frames);
    printf("\n");
}

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        showHelp();
        return 0;
    }
    QMContext qmctx;
    get_default_qmctx(&qmctx);
    parse_cmds(argc, argv, &qmctx);

    if (strlen(qmctx.s_out_fname) > 0)
    {
        qmctx.out_file = fopen(qmctx.s_out_fname, "w");
        if (NULL == qmctx.out_file)
        {
            fprintf(stderr, "Open output file %s error!\n", qmctx.s_out_fname);
            return -1;
        }
    }

    show_parameters(&qmctx);

    if (qmctx.i_threads > 1)
        process_quality_metric_multithread(&qmctx);
    else
        process_quality_metric_singlethread(&qmctx);

    fclose(qmctx.out_file);
    return 1;
}