// stereo_host: embedded images -> FPGA kernel -> verify + time.
// Usage on board:  ./stereo_host -x stereo.bin

#include "stereo.h"
#include "left_img.h"    // provides: unsigned char left_bin[];  unsigned int left_bin_len;
#include "right_img.h"   // provides: unsigned char right_bin[]; unsigned int right_bin_len;

#include <xrt/xrt_device.h>
#include <xrt/xrt_kernel.h>
#include <xrt/xrt_bo.h>

#include <fstream>
#include <vector>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string xclbinPath = "stereo.bin";
    for (int i = 1; i < argc - 1; ++i)
        if (std::string(argv[i]) == "-x") xclbinPath = argv[i + 1];

    // 1. Inputs are compiled in — just sanity-check the sizes
    if (left_bin_len != IMG_SIZE || right_bin_len != IMG_SIZE) {
        std::cerr << "ERROR: embedded image size mismatch (expected "
                  << IMG_SIZE << ", got " << left_bin_len << "/"
                  << right_bin_len << ")\n";
        return 1;
    }
    std::vector<uint8_t> out(IMG_SIZE);

    // 2. Device, xclbin, kernel
    auto device = xrt::device(0);
    auto uuid   = device.load_xclbin(xclbinPath);
    auto krnl   = xrt::kernel(device, uuid, "stereo_accel");

    // 3. Buffers on the kernel's memory banks
    auto bo_l = xrt::bo(device, IMG_SIZE, krnl.group_id(0));
    auto bo_r = xrt::bo(device, IMG_SIZE, krnl.group_id(1));
    auto bo_o = xrt::bo(device, IMG_SIZE, krnl.group_id(2));

    // 4. Send inputs to device
    bo_l.write(left_bin);
    bo_r.write(right_bin);
    bo_l.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    bo_r.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    // 5. Run (best of 10 warm runs)
    double bestMs = 1e9;
    for (int i = 0; i < 10; ++i) {
        auto t0 = std::chrono::steady_clock::now();
        auto run = krnl(bo_l, bo_r, bo_o);
        run.wait();
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (ms < bestMs) bestMs = ms;
    }

    // 6. Fetch result
    bo_o.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    bo_o.read(out.data());

    // 7. Verify (dummy copies left -> out) and save for inspection
    bool ok = (std::memcmp(out.data(), left_bin, IMG_SIZE) == 0);
    std::ofstream("disp_fpga.bin", std::ios::binary)
        .write(reinterpret_cast<char*>(out.data()), IMG_SIZE);

    std::cout << (ok ? "ROUND TRIP OK" : "ROUND TRIP FAILED")
              << " | kernel best of 10: " << bestMs << " ms\n";
    return ok ? 0 : 1;
}
