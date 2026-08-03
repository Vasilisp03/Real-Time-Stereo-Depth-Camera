// xf_sgbm_adcensus_lut.h
//
// AD-Census fusion LUTs for xFSGBMcomputecost.
//
// rho(c, lambda) = 1 - exp(-c/lambda), precomputed and scaled to [0,127]
// (NOT [0,255]) so that rho_census_lut[census_cost] + rho_ad_lut[ad_cost]
// fits in a single uint8_t (max 254) without changing the width of the
// existing _cost stream or anything in xFSGBMoptimization downstream.
//
// Values below were validated against the Middlebury MiddEval3 quarter-res
// training set (15 scenes): AD-Census fusion at these lambdas reduced mean
// avgErr from 12.46px (census-only) to 8.94px, and bad0.5 from 71.4% to
// 68.5%, averaged across all 15 scenes -- though note per-scene results
// varied significantly, with the biggest wins on low-texture scenes
// (Teddy, Recycle, PlaytableP) and regressions on scenes with deliberate
// left/right exposure mismatch (ArtL, MotorcycleE, PianoL) -- AD is not
// robust to radiometric mismatch between cameras, which is exactly the
// failure mode census transform is otherwise immune to. If your target
// hardware setup has well-matched left/right camera exposure, this is
// less of a concern; if not, consider retuning lambda_ad higher (see
// notes in the accompanying conversation) before deploying.
//
// To retune: rerun the LUT-generation script with new lambda values and
// replace the arrays below. Domain sizes (25 for census, 256 for AD) are
// fixed by the cost ranges themselves (5x5 census window = 24 comparison
// bits max Hamming distance; AD on 8-bit pixels = 0..255 max difference)
// and don't need to change unless the census window size itself changes.
#ifndef _XF_SGBM_ADCENSUS_LUT_H_
#define _XF_SGBM_ADCENSUS_LUT_H_
// rho_census_lut: lambda_census = 10.0, domain 0..24, scaled to max 127
static const ap_uint<7> rho_census_lut[25] = {
    0, 12, 23, 33, 42, 50, 57, 64, 70, 75, 80, 85, 89, 92, 96, 99, 101, 104, 106, 108, 110, 111, 113, 114, 115
};
// rho_ad_lut: lambda_ad = 30.0, domain 0..255, scaled to max 127
static const ap_uint<7> rho_ad_lut[256] = {
    0, 4, 8, 12, 16, 19, 23, 26, 30, 33, 36, 39, 42, 45, 47, 50,
    52, 55, 57, 60, 62, 64, 66, 68, 70, 72, 74, 75, 77, 79, 80, 82,
    83, 85, 86, 87, 89, 90, 91, 92, 94, 95, 96, 97, 98, 99, 100, 100,
    101, 102, 103, 104, 105, 105, 106, 107, 107, 108, 109, 109, 110, 110, 111, 111,
    112, 112, 113, 113, 114, 114, 115, 115, 115, 116, 116, 117, 117, 117, 118, 118,
    118, 118, 119, 119, 119, 120, 120, 120, 120, 120, 121, 121, 121, 121, 121, 122,
    122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124,
    124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125,
    125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
    126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126,
    126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
};
#endif
