#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

typedef long long      int64_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
#define MAX  0x7FFFFFFF

enum {
    CIDX_Y = 0,
    CIDX_U = 1,
    CIDX_V = 2,
    CIDX_CHROMA = 1,
};

enum {
    YUV420 = 0,
    YUV422 = 1,
    YUV444 = 2,
};


typedef struct yuv_frame {
    int   width[3];
    int   height[3];
    int   bit_depth;
    int   chroma_format;
    int   pixel_size;
    int   frame_size;  // frame size in bytes
    int   y_size;
    int   uv_size;
    unsigned char* yuv[3];
}frame, Frame;

int alloc_frame(Frame* f, int width, int height, int bit_depth, int chroma_format)
{
    unsigned char* yuv_buf = NULL;
    f->width[CIDX_Y]       = width;
    f->height[CIDX_Y]      = height;
    f->chroma_format       = chroma_format;
    if (f->chroma_format == YUV420)
    {
        f->width[CIDX_U]  = f->width[CIDX_V]  = width / 2;
        f->height[CIDX_U] = f->height[CIDX_V] = height / 2;
    }
    else if (f->chroma_format == YUV422)
    {
        f->width[CIDX_U]  = f->width[CIDX_V]  = width / 2;
        f->height[CIDX_U] = f->height[CIDX_V] = height;
    }
    else if (f->chroma_format == YUV444)
    {
        f->width[CIDX_U]  = f->width[CIDX_V]  = width;
        f->height[CIDX_U] = f->height[CIDX_V] = height;
    }
    f->bit_depth     = bit_depth;
    f->pixel_size    = f->bit_depth == 8 ? 1 : 2;
    f->y_size        = f->width[CIDX_Y] * f->height[CIDX_Y] * f->pixel_size;
    f->uv_size       = f->width[CIDX_CHROMA] * f->height[CIDX_CHROMA] * f->pixel_size;
    f->frame_size    = f->y_size + 2 * f->uv_size;
    yuv_buf = (char *)malloc(f->frame_size);
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

enum {
    ARG_EXE = 0,
    ARG_REFYUV = 1,
    ARG_DSTYUV = 2,
    ARG_WIDTH = 3,
    ARG_HEIGHT = 4,
    ARG_BITDEPTH = 5,
    ARG_CHROMAFMT = 6,
    ARG_FRAMENUM = 7,
    ARG_SKIPREFYUV = 8,
    ARG_SKIPDSTYUV = 9,
    ARG_TOTAL_NUM,
};

void show_help(void)
{
    printf("Calculate psnr for 8-bit or 10-bit yuv.\n");
    printf("Usage:\n");
    printf("psnr_calc.exe  ref.yuv  dst.yuv  width  height  [bit_depth]  [chroma_format]  [frame_num]  [ref_skip_frame_num]  [dst_skip_frame_num]\n");
    printf("    ref.yuv            -- set reference yuv\n");
    printf("    dst.yuv            -- set destination yuv\n");
    printf("    width              -- set yuv width\n");
    printf("    height             -- set yuv height\n"); 
    printf("    bit_depth          -- optional parameter, set bit depth of yuv.                                 Default 8\n");
    printf("    chroma_format      -- optional parameter, chroma-format. 0 - YUV420; 1 - YUV422; 2 - YUV444.    Default 0\n");
    printf("    frame_num          -- optional parameter, set frame number to calculate psnr.                   Default MAX\n");
    printf("    ref_skip_frame_num -- optional parameter, set skip how many frames of the ref yuv.              Default 0\n");
    printf("    dst_skip_frame_num -- optional parameter, set skip how many frames of the dst yuv.              Default 0\n");
}

void show_parameters(char* str_ref, char* str_dst, int width, int height, int bit_depth, int chroma_fmt, int frame_num, int ref_skip_frame_num, int dst_skip_frame_num)
{
    printf("ref yuv:            %s\n", str_ref);
    printf("dst yuv:            %s\n", str_dst);
    printf("width  :            %d\n", width);
    printf("height :            %d\n", height);
    printf("bit_depth:          %d\n", bit_depth);
    printf("chroma_format:      %d\n", chroma_fmt);
    printf("frame_num:          %d\n", frame_num);
    printf("ref_skip_frame_num: %d\n", ref_skip_frame_num);
    printf("dst_skip_frame_num: %d\n", dst_skip_frame_num);
}

int main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        show_help();
        return 0;
    }
    else if (argc <= ARG_HEIGHT)
    {
        printf("Wrong parameters!\n");
        show_help();
        return 0;
    }
    FILE* ref_file, *dst_file;
    Frame ref_frame, dst_frame;
    int64_t frame_ssd[3];
    double  frame_psnr[3];
    double  avg_psnr[3] = { 0.0, 0.0, 0.0 };
    double  pixel_max_ssd = 0;
    int width = 1920, height = 1080, bit_depth = 8, chroma_format = YUV420;
    int frame_num = MAX, skip_ref_frames = 0, skip_dst_frames = 0;
    int i;

    ref_file = fopen(argv[ARG_REFYUV], "rb");
    if (NULL == ref_file)
    {
        printf("Open ref yuv file %s error!\n", argv[ARG_REFYUV]);
        return -1;
    }
    dst_file = fopen(argv[ARG_DSTYUV], "rb");
    if (NULL == ref_file)
    {
        printf("Open dst yuv file %s error!\n", argv[ARG_DSTYUV]);
        return -1;
    }
    width  = atoi(argv[ARG_WIDTH]);
    height = atoi(argv[ARG_HEIGHT]);
    if (argc > ARG_BITDEPTH)
        bit_depth = atoi(argv[ARG_BITDEPTH]);
    if (argc > ARG_CHROMAFMT)
        chroma_format = atoi(argv[ARG_CHROMAFMT]);
    if (argc > ARG_FRAMENUM)
        frame_num = atoi(argv[ARG_FRAMENUM]);
    if (argc > ARG_SKIPREFYUV)
        skip_ref_frames = atoi(argv[ARG_SKIPREFYUV]);
    if (argc > ARG_SKIPDSTYUV)
        skip_dst_frames = atoi(argv[ARG_SKIPDSTYUV]);
    show_parameters(argv[ARG_REFYUV], argv[ARG_DSTYUV], width, height, bit_depth, chroma_format, frame_num, skip_ref_frames, skip_dst_frames);

    alloc_frame(&ref_frame, width, height, bit_depth, chroma_format);
    alloc_frame(&dst_frame, width, height, bit_depth, chroma_format);

    if (jump_to_frame(ref_file, ref_frame.frame_size, skip_ref_frames) < 0)
    {
        printf("Ref yuv jump to %d frame failed!\n", skip_ref_frames);
        return 0;
    }
    if (jump_to_frame(dst_file, dst_frame.frame_size, skip_dst_frames) < 0)
    {
        printf("Dst yuv jump to %d frame failed!\n", skip_dst_frames);
        return 0;
    }

    int pixel_max_value = (1 << bit_depth) - 1;
    pixel_max_ssd = pixel_max_value * pixel_max_value;

    printf(" Frame    PSNR_Y    PSNR_U    PSNR_V\n");
    for (i = 0; i < frame_num; i++)
    {
        if (read_frame(ref_file, &ref_frame) < 0)
            break;
        if (read_frame(dst_file, &dst_frame) < 0)
            break;
        get_frame_ssd(&ref_frame, &dst_frame, frame_ssd);

        frame_psnr[CIDX_Y] = ssd_to_psnr(pixel_max_ssd * ref_frame.width[CIDX_Y] * ref_frame.height[CIDX_Y], frame_ssd[CIDX_Y]);
        frame_psnr[CIDX_U] = ssd_to_psnr(pixel_max_ssd * ref_frame.width[CIDX_CHROMA] * ref_frame.height[CIDX_CHROMA], frame_ssd[CIDX_U]);
        frame_psnr[CIDX_V] = ssd_to_psnr(pixel_max_ssd * ref_frame.width[CIDX_CHROMA] * ref_frame.height[CIDX_CHROMA], frame_ssd[CIDX_V]);
        printf("%6d    %6.3f    %6.3f    %6.3f\n", i + 1, frame_psnr[CIDX_Y], frame_psnr[CIDX_U], frame_psnr[CIDX_V]);
        avg_psnr[CIDX_Y] += frame_psnr[CIDX_Y];
        avg_psnr[CIDX_U] += frame_psnr[CIDX_U];
        avg_psnr[CIDX_V] += frame_psnr[CIDX_V];
    }

    frame_num = i == 0 ? 1 : i;
    printf("Average   %6.3f    %6.3f    %6.3f\n", avg_psnr[CIDX_Y] / frame_num, avg_psnr[CIDX_U] / frame_num, avg_psnr[CIDX_V] / frame_num);

    free_frame(&ref_frame);
    free_frame(&dst_frame);
    fclose(dst_file);
    fclose(ref_file);
    return 1;
}