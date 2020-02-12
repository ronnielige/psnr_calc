#ifndef _YUVFRAME_H_
#define _YUVFRAME_H_
#include <stdio.h>
#include <stdlib.h>

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

int  alloc_frame(Frame* f, int width, int height, int bit_depth, int chroma_format);
void free_frame(Frame* f);
int  read_frame(FILE* in_f, Frame* f);
#endif
