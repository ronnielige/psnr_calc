#ifndef _DEFINE_H_
#define _DEFINE_H_

typedef long long      int64_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef unsigned int   uint32_t;
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

enum {
    M_NONE = 0,
    M_PSNR = 1,
    M_SSIM = 2,
};

#endif