#include "quality_metric.h"
#include "getopt.h"
#include "options.h"
#include <stdlib.h>
#include <stdio.h>

void show_parameters(QMContext* qmctx)
{
    printf("ref yuv:            %s\n", qmctx->s_ref_fname);
    printf("dst yuv:            %s\n", qmctx->s_dst_fname);
    printf("width  :            %d\n", qmctx->ia_width[CIDX_Y]);
    printf("height :            %d\n", qmctx->ia_height[CIDX_Y]);
    printf("bit_depth:          %d\n", qmctx->i_bit_depth);
    printf("chroma_format:      %d\n", qmctx->i_chroma_format);
    printf("frame_num:          %d\n", qmctx->i_frame_num);
    printf("ref_skip_frame_num: %d\n", qmctx->i_ref_skip_num);
    printf("dst_skip_frame_num: %d\n", qmctx->i_dst_skip_num);
    printf("auto_skip:          %d\n", qmctx->i_auto_skip);
    printf("threads:            %d\n", qmctx->i_threads);
    printf("metric_method:      %d\n", qmctx->i_metric_method);
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
    show_parameters(&qmctx);

    FILE* ref_file, *dst_file;
    Frame ref_frame, dst_frame;
    int64_t frame_ssd[3];
    double  frame_psnr[3], frame_ssim[3];
    double  avg_psnr[3] = { 0.0, 0.0, 0.0 }, avg_ssim[3] = { 0.0, 0.0, 0.0 };
    double  pixel_max_ssd = 0;
    int*    temp;
    int i;

    ref_file = fopen(qmctx.s_ref_fname, "rb");
    if (NULL == ref_file)
    {
        printf("Open ref yuv file %s error!\n", qmctx.s_ref_fname);
        return -1;
    }
    dst_file = fopen(qmctx.s_dst_fname, "rb");
    if (NULL == ref_file)
    {
        printf("Open dst yuv file %s error!\n", qmctx.s_dst_fname);
        return -1;
    }

    alloc_frame(&ref_frame, qmctx.ia_width[CIDX_Y], qmctx.ia_height[CIDX_Y], qmctx.i_bit_depth, qmctx.i_chroma_format);
    alloc_frame(&dst_frame, qmctx.ia_width[CIDX_Y], qmctx.ia_height[CIDX_Y], qmctx.i_bit_depth, qmctx.i_chroma_format);

    if (jump_to_frame(ref_file, ref_frame.frame_size, qmctx.i_ref_skip_num) < 0)
    {
        printf("Ref yuv jump to %d frame failed!\n", qmctx.i_ref_skip_num);
        return 0;
    }
    if (jump_to_frame(dst_file, dst_frame.frame_size, qmctx.i_dst_skip_num) < 0)
    {
        printf("Dst yuv jump to %d frame failed!\n", qmctx.i_dst_skip_num);
        return 0;
    }

    int pixel_max_value = (1 << qmctx.i_bit_depth) - 1;
    int pixel_byte = (qmctx.i_bit_depth > 8 ? 2 : 1);
    pixel_max_ssd = pixel_max_value * pixel_max_value;

    //// Step 1: Show Title
    printf(" Frame    ");
    if(qmctx.i_metric_method & M_PSNR)
        printf("PSNR_Y    PSNR_U    PSNR_V    ");
    if (qmctx.i_metric_method & M_SSIM)
        printf("SSIM_Y    SSIM_U    SSIM_V    ");
    printf("\n");

    //// Step 2: Metric Quality
    temp = (int *)malloc((2 * qmctx.ia_width[CIDX_Y] + 12) * sizeof(int));
    for (i = 0; i < qmctx.i_frame_num; i++)
    {
        if (read_frame(ref_file, &ref_frame) < 0)
            break;
        if (read_frame(dst_file, &dst_frame) < 0)
            break;
        printf("%6d    ", i + 1);
        if (qmctx.i_metric_method & M_PSNR)
        {
            get_frame_ssd(&ref_frame, &dst_frame, frame_ssd);
            for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
            {
                frame_psnr[cidx] = ssd_to_psnr(pixel_max_ssd * ref_frame.width[cidx] * ref_frame.height[cidx], frame_ssd[cidx]);
                avg_psnr[cidx] += frame_psnr[cidx];
            }
            printf("%6.3f    %6.3f    %6.3f    ", frame_psnr[CIDX_Y], frame_psnr[CIDX_U], frame_psnr[CIDX_V]);
        }
        if (qmctx.i_metric_method & M_SSIM)
        {
            for (int cidx = CIDX_Y; cidx <= CIDX_V; cidx++)
            {
                frame_ssim[cidx] = ssim_plane(ref_frame.yuv[cidx], ref_frame.width[cidx] * pixel_byte,
                                              dst_frame.yuv[cidx], dst_frame.width[cidx] * pixel_byte,
                                              ref_frame.width[cidx], ref_frame.height[cidx], temp);
                avg_ssim[cidx] += frame_ssim[cidx];
            }
            printf("%6.3f    %6.3f    %6.3f    ", frame_ssim[CIDX_Y], frame_ssim[CIDX_U], frame_ssim[CIDX_V]);
        }
        printf("\n");
    }

    /// Step 3. Show Average result
    qmctx.i_frame_num = i == 0 ? 1 : i;
    printf("Average   ");
    if (qmctx.i_metric_method & M_PSNR)
        printf("%6.3f    %6.3f    %6.3f    ", avg_psnr[CIDX_Y] / qmctx.i_frame_num, avg_psnr[CIDX_U] / qmctx.i_frame_num, avg_psnr[CIDX_V] / qmctx.i_frame_num);
    if (qmctx.i_metric_method & M_SSIM)
        printf("%6.3f    %6.3f    %6.3f    ", avg_ssim[CIDX_Y] / qmctx.i_frame_num, avg_ssim[CIDX_U] / qmctx.i_frame_num, avg_ssim[CIDX_V] / qmctx.i_frame_num);
    printf("\n");

    /// Step 4. Release resource
    free_frame(&ref_frame);
    free_frame(&dst_frame);
    free(temp);
    fclose(dst_file);
    fclose(ref_file);
    return 1;
}