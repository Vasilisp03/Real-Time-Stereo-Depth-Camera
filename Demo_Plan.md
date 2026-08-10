# SGBM Acceleration & Optimization Demonstration 

## SGBM Parameters
* Image Resolution: 768 x 384
* Input: Two rectified images of the same scene
* Output: Disparity Map


## Demo Plan
1. Run the original Vitis Vision Library SGBM
   1. First in PS
      1. Run 1 frame
      2. Note the average latency over x runs of the same frame
   2. Second in hardware
      1. Run 1 frame 100 times
      2. Our script reports the average, min and max latency
2. Run our improved SGBM implementation
   1. First in PS
      1. Run 8 frames
      2. Note the latency over x runs of the same 8 frames
   1. Second in hardware
      1. Run 48 frames 100 times
      2. Our script reports the average, min and max latency
3. Inspect the results from baseline and improved SGBM in hardware
4. Inspect results from Vitis Vision SW-only and HW-only outputs
5. Inspect results from our SW-only and HW-only outputs and note they are identical
6. Run the accuracy comparison script against results of baseline and optimised code

### Shell Script: ./run_sgbm_bench.sh
Does NOT handle timing itself. This script runs the given code the specified number of times and only pulls the timing values from the timing that is done within the code.
```
./run_sgbm_bench.sh -n 100 -- ./sgbm_host left.png right.png
```

## Results
### Original Vitis Vision SGBM:
- SW:
  - Min: 14528.5 ms
  - Max: 20217.6 ms
  - Avg: 19646.0 ms
- HW:
  - Min: 13.2 ms
  - Max: 14.3 ms
  - Avg: 13.3 ms
  - Total bad pixels: 43.9%
  - Avg error: 18.4%
- HW Speedup over SW: ~1501x
### Our SGBM:
- SW:
  - Min: 76967.8 ms (Only ran once)
  - Max: 76967.8 ms (Only ran once)
  - Avg: 76967.8 ms (Only ran once)
- HW:
  - Min:
  - Max: 
  - Avg: 17.762 ms
  - Total bad pixels:
  - Avg error:
- HW Speedup over SW: ~4333x
### Overall:
- Our HW speedup over Vitis Vision HW: 0.75x
- Our SW speedup over Vitis Vision SW: 0.25x
