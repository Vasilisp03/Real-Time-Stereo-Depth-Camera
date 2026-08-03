ad census changes:

1. New file: xf_sgbm_adcensus_lut. Needs to sit next to xf_sgbm.hpp in same directory.

2. xf_sgbm.hpp — new #include
   Added `#include "xf_sgbm_adcensus_lut.h"` near the top.

3. xFSGBMcomputecost. this is the algorithm change
  

4.  xFProcessCensusTransform5x5 and xFCensus5x5 (census transform file)
   Since the cost function needs the raw pixel too, the census transform's output word now packs both values into the existing 32-bit word: raw pixel in the upper 8 bits, 24-bit census value in the lower bits. All three _dst_mat.write(...) calls changed from writing just the census value to writing the packed word.

5. SemiGlobalBM (top-level wrapper)
   - Added two new streams: _src_raw_l, _src_raw_r
   - The loop that used to just forward the 24-bit census value now also splits out the raw pixel: reads the packed 32-bit word, writes bits [23:0] to _src_census24_l/r and bits [31:24] to _src_raw_l/r
   - Updated the call to xFSGBMcomputecost to pass the two new streams
