#include "stereo.h"

extern "C" void stereo_accel(const uint8_t* img_l,
                             const uint8_t* img_r,
                             uint8_t*       disp_out) {
#pragma HLS INTERFACE m_axi     port=img_l    offset=slave bundle=gmem0 depth=370500
#pragma HLS INTERFACE m_axi     port=img_r    offset=slave bundle=gmem1 depth=370500
#pragma HLS INTERFACE m_axi     port=disp_out offset=slave bundle=gmem2 depth=370500
#pragma HLS INTERFACE s_axilite port=return

    // W5 dummy: copy left image to output — proves full-frame data movement.
    copy_loop: for (int i = 0; i < IMG_SIZE; ++i) {
#pragma HLS PIPELINE II=1
        disp_out[i] = img_l[i];
    }
}
