/*
 * Batch wrapper for xf_sgbm.hpp.
 * Keeps one DATAFLOW region alive across multiple frames so adjacent frames
 * can occupy different SGBM stages concurrently.
 */
#ifndef _XF_SGBM_BATCH_HPP_
#define _XF_SGBM_BATCH_HPP_

#include "imgproc/xf_sgbm.hpp"

namespace xf {
namespace cv {

template <int ID,
          int ROWS, int COLS, int DEPTH_SRC, int DEPTH_DST, int NPC,
          int XFCVDEPTH_IN, int WORDWIDTH_SRC, int WORDWIDTH_DST>
void xFCensusTransformBatch(
    hls::stream<XF_SNAME(WORDWIDTH_SRC)>& src,
    hls::stream<XF_SNAME(WORDWIDTH_DST)>& dst,
    uint8_t window_size,
    uint8_t border_type,
    int height,
    int width,
    int frame_count) {
#pragma HLS INLINE OFF
frame_loop:
    for (int frame = 0; frame < frame_count; ++frame) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16
#pragma HLS PIPELINE II=1
        // Each call creates/resets the Census line-buffer state for one frame.
        xFCensusTransformKernel<ROWS, COLS, DEPTH_SRC, DEPTH_DST, NPC,
                                XFCVDEPTH_IN, WORDWIDTH_SRC, WORDWIDTH_DST>(
            src, dst, window_size, border_type, height, width);
    }
}

template <int ROWS, int COLS>
void xFCensusUnpackBatch(
    hls::stream<ap_uint<32> >& census_l,
    hls::stream<ap_uint<32> >& census_r,
    hls::stream<ap_uint<24> >& census24_l,
    hls::stream<ap_uint<24> >& census24_r,
    hls::stream<ap_uint<8> >& raw_l,
    hls::stream<ap_uint<8> >& raw_r,
    int height,
    int width,
    int frame_count) {
#pragma HLS INLINE OFF

frame_loop:
    for (int frame = 0; frame < frame_count; ++frame) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16

    row_loop:
        for (int r = 0; r < height; ++r) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=ROWS

        col_loop:
            for (int c = 0; c < width; ++c) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=COLS
#pragma HLS PIPELINE II=1

                ap_uint<32> packed_l = census_l.read();
                ap_uint<32> packed_r = census_r.read();

                census24_l.write(packed_l.range(23, 0));
                census24_r.write(packed_r.range(23, 0));

                raw_l.write(packed_l.range(31, 24));
                raw_r.write(packed_r.range(31, 24));
            }
        }
    }
}
template <int NDISP, int PU, int ROWS, int COLS>
void xFSGBMComputeCostBatch(
    hls::stream<ap_uint<24> >& census24_l,
    hls::stream<ap_uint<24> >& census24_r,
    hls::stream<ap_uint<8> >& raw_l,
    hls::stream<ap_uint<8> >& raw_r,
    hls::stream<ap_uint<8> > cost[PU],
    int height,
    int width,
    int frame_count) {
#pragma HLS INLINE OFF
#pragma HLS ARRAY_PARTITION variable=cost complete dim=1

frame_loop:
    for (int frame = 0; frame < frame_count; ++frame) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16

        xFSGBMcomputecost<NDISP, PU, ROWS, COLS>(
            census24_l,
            census24_r,
            raw_l,
            raw_r,
            cost,
            height,
            width);
    }
}

template <int NDISP, int PU, int R, int ROWS, int COLS>
void xFSGBMOptimizationBatch(
    hls::stream<ap_uint<8> > cost[PU],
    hls::stream<ap_uint<16> > agg_cost[PU],
    int height,
    int width,
    uint8_t p1,
    uint8_t p2,
    int frame_count) {
#pragma HLS INLINE OFF
#pragma HLS ARRAY_PARTITION variable=cost complete dim=1
#pragma HLS ARRAY_PARTITION variable=agg_cost complete dim=1
frame_loop:
    for (int frame = 0; frame < frame_count; ++frame) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16
#pragma HLS PIPELINE II=1
        // The original function initializes Lr/Lr_min for every invocation,
        // keeping directional state isolated between frames.
        xFSGBMoptimization<NDISP, PU, R, ROWS, COLS>(
            cost, agg_cost, height, width, p1, p2);
    }
}

template <int NDISP, int PU, int ROWS, int COLS>
void xFSGBMDisparityBatch(
    hls::stream<ap_uint<16> > agg_cost[PU],
    hls::stream<ap_uint<16> >& dst,
    int height,
    int width,
    int frame_count) {
#pragma HLS INLINE OFF
#pragma HLS ARRAY_PARTITION variable=agg_cost complete dim=1
frame_loop:
    for (int frame = 0; frame < frame_count; ++frame) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=16
#pragma HLS PIPELINE II=1
        xfSGBMcomputedisparity<NDISP, PU, ROWS, COLS>(
            agg_cost, dst, height, width);
    }
}

/*
 * src_l/src_r must contain frame_count consecutive raster-order frames.
 * dst emits frame_count consecutive raster-order disparity frames.
 * Frame boundaries are count-based: exactly height*width pixels per frame.
 */
template <int BORDER_TYPE,
          int WINDOW_SIZE,
          int NDISP,
          int PU,
          int R,
          int SRC_T,
          int DST_T,
          int ROWS,
          int COLS,
          int NPC,
          int XFCVDEPTH_IN_L = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_IN_R = _XFCVDEPTH_DEFAULT,
          int XFCVDEPTH_OUT = _XFCVDEPTH_DEFAULT>
void SemiGlobalBMBatch(
    hls::stream<XF_TNAME(SRC_T, NPC)>& src_l,
    hls::stream<XF_TNAME(SRC_T, NPC)>& src_r,
    hls::stream<XF_TNAME(DST_T, NPC)>& dst,
    uint8_t p1,
    uint8_t p2,
    int height,
    int width,
    int frame_count) {
#pragma HLS INLINE OFF

#ifndef _SYNTHESIS_
    assert((SRC_T == XF_8UC1) && "SRC_T must be XF_8UC1");
    assert((DST_T == XF_16UC1) && "DST_T must be XF_16UC1");
    assert((NPC == XF_NPPC1) && "NPC must be XF_NPPC1");
    assert((WINDOW_SIZE == 5) && "WINDOW_SIZE must be 5");
    assert((NDISP > 1 && NDISP <= 256) && "Invalid NDISP");
    assert((NDISP >= PU && ((NDISP / PU) * PU == NDISP)) &&
           "NDISP must be divisible by PU");
    assert(((R == 2) || (R == 3) || (R == 4)) && "R must be 2, 3, or 4");
    assert((p1 < p2) && (p2 <= 100));
    assert((height <= ROWS) && (width <= COLS));
    assert(frame_count > 0);
#endif

    hls::stream<ap_uint<32> > census_l("census_l");
    hls::stream<ap_uint<32> > census_r("census_r");
    hls::stream<ap_uint<24> > census24_l("census24_l");
    hls::stream<ap_uint<24> > census24_r("census24_r");
    hls::stream<ap_uint<8> > cost[PU];
    hls::stream<ap_uint<16> > agg_cost[PU];
    hls::stream<ap_uint<8> > raw_l("raw_l");
    hls::stream<ap_uint<8> > raw_r("raw_r");

#pragma HLS STREAM variable=census_l depth=64
#pragma HLS STREAM variable=census_r depth=64
#pragma HLS STREAM variable=census24_l depth=64
#pragma HLS STREAM variable=census24_r depth=64
#pragma HLS STREAM variable=cost depth=64
#pragma HLS STREAM variable=agg_cost depth=64
#pragma HLS STREAM variable=raw_l depth=64
#pragma HLS STREAM variable=raw_r depth=64
#pragma HLS ARRAY_PARTITION variable=cost complete dim=1
#pragma HLS ARRAY_PARTITION variable=agg_cost complete dim=1

#pragma HLS DATAFLOW

    // ID distinguishes the two Census process instances during synthesis.
    xFCensusTransformBatch<0, ROWS, COLS,
                            XF_DEPTH(SRC_T, NPC), XF_DEPTH(XF_32UC1, NPC), NPC,
                            XFCVDEPTH_IN_L,
                            XF_WORDWIDTH(SRC_T, NPC), XF_WORDWIDTH(XF_32UC1, NPC)>(
        src_l, census_l, WINDOW_SIZE, BORDER_TYPE, height, width, frame_count);

    xFCensusTransformBatch<1, ROWS, COLS,
                            XF_DEPTH(SRC_T, NPC), XF_DEPTH(XF_32UC1, NPC), NPC,
                            XFCVDEPTH_IN_R,
                            XF_WORDWIDTH(SRC_T, NPC), XF_WORDWIDTH(XF_32UC1, NPC)>(
        src_r, census_r, WINDOW_SIZE, BORDER_TYPE, height, width, frame_count);

    xFCensusUnpackBatch<ROWS, COLS>(
        census_l,
        census_r,
        census24_l,
        census24_r,
        raw_l,
        raw_r,
        height,
        width,
        frame_count);

    xFSGBMComputeCostBatch<NDISP, PU, ROWS, COLS>(
        census24_l,
        census24_r,
        raw_l,
        raw_r,
        cost,
        height,
        width,
        frame_count);

    xFSGBMOptimizationBatch<NDISP, PU, R, ROWS, COLS>(
        cost, agg_cost, height, width, p1, p2, frame_count);

    xFSGBMDisparityBatch<NDISP, PU, ROWS, COLS>(
        agg_cost, dst, height, width, frame_count);
}

} // namespace cv
} // namespace xf

#endif // _XF_SGBM_BATCH_HPP_
