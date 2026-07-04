// disp_diff.cpp -- binary bad-pixel map between a disparity result and ground truth.
//
// Reads two Middlebury PFM disparity maps (float, 1-channel "Pf", inf = unknown),
// optionally a mask0nocc.png (nonocc mask), computes per-pixel absolute error,
// prints summary stats (bad%, avgErr, rms) matching runeval, and writes a binary
// bad-pixel map as a PNG (white = bad, black = good, gray = not evaluated).
//
// Build:  g++ -O2 -o disp_diff disp_diff.cpp -lpng
// Usage:  ./disp_diff RESULT.pfm GT.pfm [--mask mask0nocc.png] [--thresh 1.0] [--out bad.png]
//
// Example:
//   ./disp_diff trainingF/Motorcycle/disp0ELAS.pfm \
//               trainingF/Motorcycle/disp0GT.pfm \
//               --mask trainingF/Motorcycle/mask0nocc.png \
//               --thresh 1.0 --out trainingF/Motorcycle/disp0ELAS-bad.png

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <png.h>

struct PFM {
    int w = 0, h = 0;
    std::vector<float> data;  // row-major, top-to-bottom
}; //pfm file structure https://www.pauldebevec.com/Research/HDR/PFM/

// ---- PFM reader (Middlebury: "Pf" 1-channel float, inf=unknown, bottom-to-top) ----
static bool readPFM(const char* path, PFM& out) {
    FILE* f = fopen(path, "rb"); //open the file
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }

    char hdr[3] = {0}; //information about the file type
    if (fscanf(f, "%2s", hdr) != 1) { fclose(f); return false; }
    bool color = (strcmp(hdr, "PF") == 0);
    if (!color && strcmp(hdr, "Pf") != 0) {
        fprintf(stderr, "%s: not a PFM (header=%s)\n", path, hdr);
        fclose(f); return false;
    }
    int w, h; float scale; //width and height
    if (fscanf(f, "%d %d", &w, &h) != 2) { fclose(f); return false; }
    if (fscanf(f, "%f", &scale) != 1)    { fclose(f); return false; }
    fgetc(f);  // consume single whitespace after scale

    bool littleEndian = (scale < 0.0f);
    int ch = color ? 3 : 1;
    size_t n = (size_t)w * h * ch;
    std::vector<float> raw(n);
    if (fread(raw.data(), sizeof(float), n, f) != n) {
        fprintf(stderr, "%s: unexpected EOF\n", path); fclose(f); return false;
    }
    fclose(f);

    if (!littleEndian) {  // swap big-endian -> host little-endian
        for (size_t i = 0; i < n; ++i) {
            unsigned char* b = reinterpret_cast<unsigned char*>(&raw[i]);
            std::swap(b[0], b[3]); std::swap(b[1], b[2]);
        }
    }
    out.w = w; out.h = h;
    out.data.resize((size_t)w * h);
    for (int y = 0; y < h; ++y) {
        int src = h - 1 - y;  // flip bottom-to-top
        for (int x = 0; x < w; ++x)
            out.data[(size_t)y*w+x] = color ? raw[((size_t)src*w+x)*3]
                                            : raw[(size_t)src*w+x];
    }
    return true;
}

// ---- read an 8-bit mask PNG into a 0/1 grid (nonzero -> evaluate) ----
static bool readMaskPNG(const char* path, int W, int H, std::vector<unsigned char>& mask) {
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open mask %s\n", path); return false; }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) { png_destroy_read_struct(&png,&info,nullptr); fclose(f); return false; }
    png_init_io(png, f);
    png_read_info(png, info);
    int w = png_get_image_width(png, info);
    int h = png_get_image_height(png, info);
    png_byte ctype = png_get_color_type(png, info);
    png_byte depth = png_get_bit_depth(png, info);
    if (depth == 16) png_set_strip_16(png);
    if (ctype == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    png_read_update_info(png, info);
    ctype = png_get_color_type(png, info);
    int chans = png_get_channels(png, info);

    std::vector<png_bytep> rows(h);
    std::vector<unsigned char> buf((size_t)w*h*chans);
    for (int y=0;y<h;++y) rows[y] = buf.data()+(size_t)y*w*chans;
    png_read_image(png, rows.data());
    png_destroy_read_struct(&png,&info,nullptr);
    fclose(f);

    if (w!=W || h!=H) {
        fprintf(stderr,"mask size %dx%d != GT %dx%d; ignoring mask\n",w,h,W,H);
        return false;
    }
    mask.assign((size_t)W*H,0);
    for (int y=0;y<H;++y) for (int x=0;x<W;++x)
        mask[(size_t)y*W+x] = buf[((size_t)y*w+x)*chans] == 255 ? 1 : 0;  // first channel
    return true;
}

// ---- write an RGB image as PNG ----
static bool writePNG(const char* path, int W, int H, const std::vector<unsigned char>& rgb) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr,"cannot write %s\n",path); return false; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) { png_destroy_write_struct(&png,&info); fclose(f); return false; }
    png_init_io(png, f);
    png_set_IHDR(png, info, W, H, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    std::vector<png_bytep> rows(H);
    for (int y=0;y<H;++y) rows[y] = const_cast<png_bytep>(rgb.data()+(size_t)y*W*3);
    png_write_image(png, rows.data());
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 3) { //check if there is the correct number of arguments
        fprintf(stderr,"usage: %s RESULT.pfm GT.pfm [--mask m.png] [--thresh T] [--out bad.png]\n",argv[0]);
        return 1;
    }
    const char* resPath = argv[1]; //path for result file
    const char* gtPath  = argv[2]; //path for ground truth
    const char* maskPath = nullptr; //mask path
    float thresh = 1.0f; //treshold
    std::string outPath = "disp-bad.png"; //default name (binary bad-pixel map)
    for (int i=3;i<argc;++i) { //iterate over remaining arguments
        if (!strcmp(argv[i],"--mask") && i+1<argc) maskPath=argv[++i]; //nocc mask file name (for pixels that are only present in one image)
        else if (!strcmp(argv[i],"--thresh") && i+1<argc) thresh=atof(argv[++i]); //treshold value
        else if (!strcmp(argv[i],"--out") && i+1<argc) outPath=argv[++i]; //output file name (bad-pixel map)
    }

    PFM res, gt; //pfm file handles
    if (!readPFM(resPath,res)) return 1; //try to read res file
    if (!readPFM(gtPath,gt))   return 1; //try to read ground truth file
    if (res.w!=gt.w || res.h!=gt.h) { //check if the images have the same size
        fprintf(stderr,"ERROR: size mismatch: result %dx%d vs GT %dx%d\n",res.w,res.h,gt.w,gt.h);
        return 1;
    }
    const int W=gt.w, H=gt.h; //image dimensions

    std::vector<unsigned char> nocc;
    bool haveMask=false;
    if (maskPath) haveMask = readMaskPNG(maskPath,W,H,nocc); //read the mask if it has been provided

    // per-pixel state for the binary map: 0=good, 1=bad, 2=not evaluated
    std::vector<unsigned char> state((size_t)W*H, 2); // default = not evaluated
    long bad=0, valid=0; double sumAbs=0, sumSq=0; // accumulators: bad-pixel count, evaluated count, sum of errors, sum of squared errors

    for (int y=0;y<H;++y) for (int x=0;x<W;++x) { ///iterate over all pixels
        size_t i=(size_t)y*W+x; //get 1D position of the pixel
        float g=gt.data[i], r=res.data[i]; //get corresponding pixels
        if (!(std::isfinite(g) && std::isfinite(r))) continue; //if pixels aren't valid, leave as "not evaluated"
        if (haveMask && !nocc[i]) continue;    // honor nonocc mask, leave as "not evaluated"
        float e=std::fabs(r-g); //find the difference
        sumAbs+=e; sumSq+=(double)e*e; //calculate values
        if (e>thresh) { state[i]=1; bad++; } //bad pixel (error over threshold)
        else          { state[i]=0; }        //good pixel
        valid++; //increment valid counter
    }
    if (!valid) { fprintf(stderr,"ERROR: no valid pixels.\n"); return 1; }

    //print values
    double badPct=100.0*bad/valid, avg=sumAbs/valid, rms=std::sqrt(sumSq/valid);
    printf("mask             : %s\n", haveMask ? "nonocc (from mask0nocc.png)" : "all valid GT");
    printf("evaluated pixels : %.1f%% of image\n", 100.0*valid/((double)W*H));
    printf("bad%-5.1f        : %.2f%%   (err > %.1f px)\n", thresh, badPct, thresh);
    printf("avgErr           : %.2f px\n", avg);
    printf("rms              : %.2f px\n", rms);

    // ---- binary bad-pixel map (white=bad, black=good, gray=not evaluated) ----
    //https://arxiv.org/pdf/1912.01306 - the idea borrowed from here
    std::vector<unsigned char> rgb((size_t)W*H*3); //buffer for the bad-pixel map
    for (int y=0;y<H;++y) for (int x=0;x<W;++x) {
        size_t i=(size_t)y*W+x;
        unsigned char v;
        if      (state[i]==2) v=64;   // not evaluated -> gray
        else if (state[i]==1) v=255;  // bad pixel     -> white
        else                  v=0;    // good pixel    -> black
        rgb[i*3+0]=v; rgb[i*3+1]=v; rgb[i*3+2]=v;
    }
    if (!writePNG(outPath.c_str(),W,H,rgb)) return 1;

    printf("\nwrote bad-pixel map -> %s  (white = err > %.1f px)\n", outPath.c_str(), thresh);
    printf("white = bad, black = good, gray = not evaluated\n");
    return 0;
}