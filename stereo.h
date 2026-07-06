#ifndef STEREO_H
#define STEREO_H

#include <stdint.h>

#define WIDTH    741
#define HEIGHT   500
#define IMG_SIZE (WIDTH * HEIGHT)   // 370500 — matches your .bin files

extern "C" void stereo_accel(const uint8_t* img_l,
                             const uint8_t* img_r,
                             uint8_t*       disp_out);

#endif
