#ifndef AS265CLI_H
#define AS265CLI_H
#include "defines.h"
#include "version.h"

static const char short_options[] = "o:D:p:f:I:hwV?";
static const struct option long_options[] = 
{
    { "help",                 no_argument, NULL, 'h' },
    { "version",              no_argument, NULL, 'V' },
    { "ref",            required_argument, NULL, 0 },
    { "dst",            required_argument, NULL, 0 },
    { "bitdepth",       required_argument, NULL, 0 },
    { "output",         required_argument, NULL, 'o' },
    { "width",          required_argument, NULL, 0 },
    { "height",         required_argument, NULL, 0 },
    { "frames",         required_argument, NULL, 0 },
    { "chroma-format",  required_argument, NULL, 0 },
    { "ref-skip-num",   required_argument, NULL, 0 },
    { "dst-skip-num",   required_argument, NULL, 0 },
    { "auto-skip",      required_argument, NULL, 0 },
    { "threads",        required_argument, NULL, 0 },
    { "metric-method",  required_argument, NULL, 1 },
    { 0, 0, 0, 0 },
};

static void showHelp(void)
{
    printf("Quality Metric Tool (Calculate psnr or ssim for yuvs) - Version %d.%d.%d.%d\n", VER_MAJOR, VER_MINOR, VER_RELEASE, VER_BUILD);
    printf("Usage: quality_metric.exe [--option opt_value]\n");
    printf("\nExecutable Options\n");
    printf("   -h/--help                   show help text and exit\n");
    printf("\nOptions:\n");
    printf("   --ref                       reference yuv input file name\n");
    printf("   --dst                       dst       yuv input file name\n");
    printf("   --bitdepth                  bitdepth of yuv input file. 8 or 10. default 8\n");
    printf("   --width                     source picture width\n");
    printf("   --height                    source picture height\n");
    printf("   --frames                    number of frames to metric. default 99999\n");
    printf("   --chroma-format             0: YUV400; 1: YUV420; 2: YUV422; 3: YUV444. default 1(YUV420)\n");
    printf("   --ref-skip-num              skip how many frames of the ref yuv. default 0\n");
    printf("   --dst-skip-num              skip how many frames of the dst yuv. default 0\n");
    printf("   --auto-skip                 auto decide skip how many frames of ref yuv and dst yuv, may be inaccurate. default 0\n");           
    printf("   --output                    output result file name\n");
    printf("   --threads                   Thread number (multi-thread not supported). default 1\n");
    printf("   --metric-method             Quality Metric method: 1 - psnr; 2 - ssim; 3 - psnr + ssim. default 1");
    printf("\n");
}
#endif