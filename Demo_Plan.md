# SGBM Acceleration & Optimization Demonstration 

## SGBM Parameters
* Image Resolution: 768 x 384
* Input: Two rectified images of the same scene
* Output: Disparity Map


## Demo Plan
1. Run the original Vitis Vision Library SGBM.
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
3. Inspect the results and note the speedup
4. Run the accuracy comparison script against results of baseline and optimised code

### Shell Script: ./run_sgbm_bench.sh
Does NOT handle timing itself. This script runs the given code the specified number of times and only pulls the timing values from the timing that is done within the code.
```
./run_sgbm_bench.sh -n 100 -- ./sgbm_host left.png right.png
```
