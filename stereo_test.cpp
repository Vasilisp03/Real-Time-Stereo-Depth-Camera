#include "stereo.h"
#include <cstdio>
#include <cstring>

static uint8_t left[IMG_SIZE], right[IMG_SIZE], out[IMG_SIZE];

static bool loadRaw(const char* path, uint8_t* buf) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(buf, 1, IMG_SIZE, f);
    fclose(f);
    return n == IMG_SIZE;
}

int main() {
    bool realData = loadRaw("left.bin", left) && loadRaw("right.bin", right);
    if (!realData) {
        printf("INFO: left.bin/right.bin not found - using synthetic pattern\n");
        for (int i = 0; i < IMG_SIZE; ++i) left[i] = (uint8_t)(i * 7);
        memcpy(right, left, IMG_SIZE);
    } else {
        printf("INFO: testing with real Middlebury images\n");
    }

    stereo_accel(left, right, out);

    if (memcmp(out, left, IMG_SIZE) != 0) {
        printf("TEST FAILED: output != input\n");
        return 1;
    }
    printf("TEST PASSED\n");
    return 0;
}
