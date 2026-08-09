#ifndef _XF_CONFIG_PARAMS_H_
#define _XF_CONFIG_PARAMS_H_

#include "ap_axi_sdata.h"
#include "ap_int.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "xf_sgbm_batch.hpp"

// Maximum supported runtime image dimensions.
#define HEIGHT 720
#define WIDTH 1280

// Maximum number of frames accepted in one kernel invocation.
#define MAX_FRAMES 1

// SGBM configuration.
#define SMALL_PENALTY 20
#define LARGE_PENALTY 40
#define WINDOW_SIZE 5
#define TOTAL_DISPARITY 64
#define PARALLEL_UNITS 32
#define NUM_DIR 4

#define IN_TYPE XF_8UC1
#define OUT_TYPE XF_16UC1
#define NPPCX XF_NPPC1

#define XF_CV_DEPTH_IN_L 2
#define XF_CV_DEPTH_IN_R 2
#define XF_CV_DEPTH_OUT 2

// Memory-mover widths used by video_mm2s_batch/video_s2mm_batch.
#define INPUT_PTR_WIDTH 32
#define OUTPUT_PTR_WIDTH 32

// One 8-bit grayscale pixel per AXI4-Stream transfer.
using VideoAxis = ap_axiu<16, 1, 1, 1>;
using InStream = hls::stream<VideoAxis>;
using OutStream = hls::stream<VideoAxis>;

extern "C" {
void semiglobalbm_accel(InStream& img_in_l,
                        InStream& img_in_r,
                        unsigned char penalty_small,
                        unsigned char penalty_large,
                        OutStream& img_out,
                        int rows,
                        int cols,
                        int frame_count);
}

#endif // _XF_CONFIG_PARAMS_H_
