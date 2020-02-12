#ifndef _DEFINE_H_
#define _DEFINE_H_

#define VER_MAJOR       1
#define VER_MINOR       0
#define VER_RELEASE     0
#define VER_BUILD       1

typedef long long      int64_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef unsigned int   uint32_t;
typedef unsigned long long uint64_t;
#define MAX  0x7FFFFFFF

enum {
    CIDX_Y = 0,
    CIDX_U = 1,
    CIDX_V = 2,
    CIDX_CHROMA = 1,
};

enum {
    YUV400 = 0,
    YUV420 = 1,
    YUV422 = 2,
    YUV444 = 3,
};

enum {
    M_NONE = 0,
    M_PSNR = 1,
    M_SSIM = 2,
};

#endif