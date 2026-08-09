#include "xf_sgbm_accel_config.h"
#include "xf_sgbm_batch.hpp"

namespace {

void axis_to_pixels(InStream& axis_in,
                    hls::stream<ap_uint<8> >& pixels,
                    int rows,
                    int cols,
                    int frame_count) {
#pragma HLS INLINE OFF
    const int total_pixels = rows * cols * frame_count;
convert_loop:
    for (int i = 0; i < total_pixels; ++i) {
#pragma HLS PIPELINE II=1
        VideoAxis v = axis_in.read();
        pixels.write((ap_uint<8>)v.data);
    }
}

void pixels_to_axis(hls::stream<ap_uint<16> >& pixels,
                    OutStream& axis_out,
                    int rows,
                    int cols,
                    int frame_count) {
#pragma HLS INLINE OFF
    const int pixels_per_frame = rows * cols;
    const int total_pixels = pixels_per_frame * frame_count;

convert_loop:
    for (int i = 0; i < total_pixels; ++i) {
#pragma HLS PIPELINE II=1
        const int pixel_in_frame = i % pixels_per_frame;
        const int col = pixel_in_frame % cols;

        VideoAxis v;
        v.data = pixels.read();
        v.keep = 1;
        v.strb = 1;
        v.user = (pixel_in_frame == 0); // SOF
        v.last = (col == cols - 1);     // EOL
        v.id = 0;
        v.dest = 0;
        axis_out.write(v);
    }
}

} // namespace

extern "C" {

void semiglobalbm_accel(InStream& img_in_l,
                        InStream& img_in_r,
                        unsigned char penalty_small,
                        unsigned char penalty_large,
                        OutStream& img_out,
                        int rows,
                        int cols,
                        int frame_count) {
#pragma HLS INTERFACE axis port=img_in_l register
#pragma HLS INTERFACE axis port=img_in_r register
#pragma HLS INTERFACE axis port=img_out register
#pragma HLS INTERFACE s_axilite port=penalty_small bundle=control
#pragma HLS INTERFACE s_axilite port=penalty_large bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=frame_count bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

#ifndef _SYNTHESIS_
    assert(rows > 0 && rows <= HEIGHT);
    assert(cols > 0 && cols <= WIDTH);
    assert(frame_count > 0 && frame_count <= MAX_FRAMES);
#endif

    hls::stream<ap_uint<8> > left_pixels("left_pixels");
    hls::stream<ap_uint<8> > right_pixels("right_pixels");
    hls::stream<ap_uint<16> > disparity_pixels("disparity_pixels");

#pragma HLS STREAM variable=left_pixels depth=64
#pragma HLS STREAM variable=right_pixels depth=64
#pragma HLS STREAM variable=disparity_pixels depth=64
#pragma HLS DATAFLOW

    axis_to_pixels(img_in_l, left_pixels, rows, cols, frame_count);
    axis_to_pixels(img_in_r, right_pixels, rows, cols, frame_count);

    xf::cv::SemiGlobalBMBatch<
        XF_BORDER_CONSTANT,
        WINDOW_SIZE,
        TOTAL_DISPARITY,
        PARALLEL_UNITS,
        NUM_DIR,
        IN_TYPE,
        OUT_TYPE,
        HEIGHT,
        WIDTH,
        NPPCX,
        XF_CV_DEPTH_IN_L,
        XF_CV_DEPTH_IN_R,
        XF_CV_DEPTH_OUT>(
            left_pixels,
            right_pixels,
            disparity_pixels,
            penalty_small,
            penalty_large,
            rows,
            cols,
            frame_count);

    pixels_to_axis(disparity_pixels, img_out, rows, cols, frame_count);
}

} // extern "C"
