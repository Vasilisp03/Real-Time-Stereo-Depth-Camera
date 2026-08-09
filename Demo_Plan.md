# SGBM Acceleration & Optimization Demonstration 

## SGBM Parameters
* Image Resolution: 768 x 384
* Input: Two rectified images of the same scene
* Output: Disparity Map


## Demo Plan
1. Run the original Vitis Vision Library, outlining our profiling metrics and I/O
2. First in Software and profile
3. Second in hardware and profile, noting speedup that Vitis Vision achieved
4. Run our code
5. First in software and profile
6. Then in hardware and profile
7. Compare the performance between the two versions
8. Run the accuracy comparison script to outline accuracy improvements

### Shell Script: ./run_sgbm_bench.sh
Does NOT handle timing itself. This script runs the given code the specified number of times and only pulls the timing values from the timing that is done within the code.
```
./run_sgbm_bench.sh -n 100 -- ./sgbm_host left.png right.png
```
