// depth_corner_full.cpp
// Pure C++ (no OpenCV) implementation of:
// Gaussian blur -> Sobel gradients -> non-max suppression -> hysteresis (Canny)
// -> direction-change-based corner detection -> corner clustering
// -> diagonal distance -> near/far classification.
//
// Accepts BMP or PGM input. Outputs Windows-native BMP files (viewable with
// Photos/Paint, no extra software). Includes a built-in synthetic test
// image generator (white square on black) used only when no file is given.
//
// Usage:
//   ./depth_corner                  -> generates a synthetic test square and processes it
//   ./depth_corner ImageTest.bmp    -> processes the given BMP file
//   ./depth_corner input.pgm        -> processes the given PGM file
//
// Build:  g++ -std=c++17 -O2 depth_corner_full.cpp -o depth_corner
// Run:    ./depth_corner ImageTest.bmp

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <queue>
#include <algorithm>
#include <string>
#include <cstdint>
#include <cctype>

// ---------------- Tunables ----------------
static const int    CANNY_LOW          = 30;
static const int    CANNY_HIGH         = 90;
static const double DEPTH_THRESHOLD    = 150.0;

// Corner detection (direction-change based)
static const int    CORNER_WINDOW      = 7;     // radius of neighborhood searched around each edge pixel
static const double CORNER_MIN_ARM_LEN = 4.0;   // min distance to each extreme point (filters noise)
static const double CORNER_MAX_ANGLE   = 135.0; // degrees; angle between arms must be < this to count as a corner
                                                  // (180 = perfectly straight line, smaller = sharper bend)
static const int    CLUSTER_RADIUS     = 15;     // merge corner candidates within this pixel radius into one point

struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> data; // row-major, 1 byte/pixel

    unsigned char& at(int x, int y) { return data[y * width + x]; }
    unsigned char  at(int x, int y) const { return data[y * width + x]; }
};

struct Point2 { int x, y; };

// ---------------- BMP writer (no external libs) ----------------
void writeBMP24(const std::string &path, int width, int height,
                 const std::vector<unsigned char> &rgb) {
    int rowSize = width * 3;
    int padding = (4 - (rowSize % 4)) % 4;
    int dataSize = (rowSize + padding) * height;
    int fileSize = 54 + dataSize;

    std::ofstream f(path, std::ios::binary);

    unsigned char fileHeader[14] = {
        'B','M', 0,0,0,0, 0,0,0,0, 54,0,0,0
    };
    fileHeader[2] = (unsigned char)(fileSize);
    fileHeader[3] = (unsigned char)(fileSize >> 8);
    fileHeader[4] = (unsigned char)(fileSize >> 16);
    fileHeader[5] = (unsigned char)(fileSize >> 24);

    unsigned char infoHeader[40] = {0};
    auto put32 = [&](unsigned char *p, int32_t v) {
        p[0] = (unsigned char)(v);
        p[1] = (unsigned char)(v >> 8);
        p[2] = (unsigned char)(v >> 16);
        p[3] = (unsigned char)(v >> 24);
    };
    put32(&infoHeader[0], 40);
    put32(&infoHeader[4], width);
    put32(&infoHeader[8], height);
    infoHeader[12] = 1; infoHeader[13] = 0;
    infoHeader[14] = 24; infoHeader[15] = 0;
    put32(&infoHeader[20], dataSize);

    f.write(reinterpret_cast<char*>(fileHeader), 14);
    f.write(reinterpret_cast<char*>(infoHeader), 40);

    std::vector<unsigned char> padBytes(padding, 0);

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            size_t i = ((size_t)y * width + x) * 3;
            unsigned char bgr[3] = { rgb[i + 2], rgb[i + 1], rgb[i] };
            f.write(reinterpret_cast<char*>(bgr), 3);
        }
        if (padding > 0) f.write(reinterpret_cast<char*>(padBytes.data()), padding);
    }
}

void writeGrayBMP(const std::string &path, const Image &img) {
    std::vector<unsigned char> rgb((size_t)img.width * img.height * 3);
    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            unsigned char v = img.at(x, y);
            size_t i = ((size_t)y * img.width + x) * 3;
            rgb[i] = rgb[i+1] = rgb[i+2] = v;
        }
    }
    writeBMP24(path, img.width, img.height, rgb);
}

// ---------------- Synthetic test image generator ----------------
void generateTestSquare(Image &img, int width = 640, int height = 480, int squareSize = 200) {
    int squareX = (width - squareSize) / 2;
    int squareY = (height - squareSize) / 2;

    img.width = width;
    img.height = height;
    img.data.assign((size_t)width * height, 0);

    for (int y = squareY; y < squareY + squareSize; y++) {
        for (int x = squareX; x < squareX + squareSize; x++) {
            img.at(x, y) = 255;
        }
    }

    std::cout << "Generated synthetic test image (" << width << "x" << height
               << "), square at (" << squareX << "," << squareY
               << ") size " << squareSize << "x" << squareSize << "\n";
}

// ---------------- PGM reader ----------------
Image readPGM(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);

    std::string magic;
    f >> magic;
    if (magic != "P5") throw std::runtime_error("Only binary PGM (P5) supported");

    auto skipComments = [&]() {
        while (isspace(f.peek())) f.get();
        while (f.peek() == '#') {
            std::string line;
            std::getline(f, line);
            while (isspace(f.peek())) f.get();
        }
    };

    skipComments();
    int w, h, maxval;
    f >> w; skipComments();
    f >> h; skipComments();
    f >> maxval;
    f.get();

    Image img;
    img.width = w;
    img.height = h;
    img.data.resize((size_t)w * h);
    f.read(reinterpret_cast<char*>(img.data.data()), img.data.size());
    return img;
}

// ---------------- BMP reader (handles 24-bit and 32-bit uncompressed BMP) ----------------
Image readBMP(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open file: " + path);

    unsigned char fileHeader[14];
    f.read(reinterpret_cast<char*>(fileHeader), 14);
    if (!f || fileHeader[0] != 'B' || fileHeader[1] != 'M')
        throw std::runtime_error("Not a valid BMP file: " + path);

    uint32_t dataOffset = fileHeader[10] | (fileHeader[11] << 8) | (fileHeader[12] << 16) | (fileHeader[13] << 24);

    unsigned char infoHeader[40];
    f.read(reinterpret_cast<char*>(infoHeader), 40);
    if (!f) throw std::runtime_error("Truncated BMP header: " + path);

    int32_t width  = infoHeader[4] | (infoHeader[5] << 8) | (infoHeader[6] << 16) | (infoHeader[7] << 24);
    int32_t heightRaw = infoHeader[8] | (infoHeader[9] << 8) | (infoHeader[10] << 16) | (infoHeader[11] << 24);
    int16_t bpp = infoHeader[14] | (infoHeader[15] << 8);
    int32_t compression = infoHeader[16] | (infoHeader[17] << 8) | (infoHeader[18] << 16) | (infoHeader[19] << 24);

    if (compression != 0)
        throw std::runtime_error("Compressed BMP not supported. Re-save as 'BMP picture' (uncompressed) in Paint.");
    if (bpp != 24 && bpp != 32)
        throw std::runtime_error("Only 24-bit or 32-bit uncompressed BMP supported (got " + std::to_string(bpp) + "-bit). Re-save from Paint as 'BMP picture'.");

    bool flipped = heightRaw > 0;
    int height = std::abs(heightRaw);
    int bytesPerPixel = bpp / 8;

    Image img;
    img.width = width;
    img.height = height;
    img.data.resize((size_t)width * height);

    int rowSize = width * bytesPerPixel;
    int padding = (4 - (rowSize % 4)) % 4;

    f.seekg(dataOffset, std::ios::beg);

    std::vector<unsigned char> rowBuf(rowSize);

    for (int row = 0; row < height; row++) {
        int y = flipped ? (height - 1 - row) : row;
        f.read(reinterpret_cast<char*>(rowBuf.data()), rowSize);
        if (!f) throw std::runtime_error("Unexpected end of BMP pixel data: " + path);

        for (int x = 0; x < width; x++) {
            unsigned char b = rowBuf[x * bytesPerPixel + 0];
            unsigned char g = rowBuf[x * bytesPerPixel + 1];
            unsigned char r = rowBuf[x * bytesPerPixel + 2];
            unsigned char gray = static_cast<unsigned char>(0.114 * b + 0.587 * g + 0.299 * r);
            img.at(x, y) = gray;
        }
        f.seekg(padding, std::ios::cur);
    }

    return img;
}

// ---------------- Step 1: Gaussian blur (5x5) ----------------
Image gaussianBlur(const Image &src) {
    static const int kernel[5][5] = {
        {2, 4, 5, 4, 2},
        {4, 9,12, 9, 4},
        {5,12,15,12, 5},
        {4, 9,12, 9, 4},
        {2, 4, 5, 4, 2}
    };
    static const int kernelSum = 159;

    Image dst;
    dst.width = src.width;
    dst.height = src.height;
    dst.data.resize(src.data.size(), 0);

    for (int y = 2; y < src.height - 2; y++) {
        for (int x = 2; x < src.width - 2; x++) {
            int sum = 0;
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    sum += src.at(x + kx, y + ky) * kernel[ky + 2][kx + 2];
                }
            }
            dst.at(x, y) = static_cast<unsigned char>(sum / kernelSum);
        }
    }
    return dst;
}

// ---------------- Step 2: Sobel gradients ----------------
struct GradResult {
    std::vector<double> magnitude;
    std::vector<double> direction;
    int width, height;
};

GradResult sobel(const Image &src) {
    static const int gx[3][3] = {{-1,0,1},{-2,0,2},{-1,0,1}};
    static const int gy[3][3] = {{-1,-2,-1},{0,0,0},{1,2,1}};

    GradResult r;
    r.width = src.width;
    r.height = src.height;
    r.magnitude.assign((size_t)src.width * src.height, 0.0);
    r.direction.assign((size_t)src.width * src.height, 0.0);

    for (int y = 1; y < src.height - 1; y++) {
        for (int x = 1; x < src.width - 1; x++) {
            int sx = 0, sy = 0;
            for (int ky = -1; ky <= 1; ky++) {
                for (int kx = -1; kx <= 1; kx++) {
                    int px = src.at(x + kx, y + ky);
                    sx += gx[ky + 1][kx + 1] * px;
                    sy += gy[ky + 1][kx + 1] * px;
                }
            }
            double mag = std::sqrt((double)(sx * sx + sy * sy));
            r.magnitude[y * src.width + x] = mag;
            r.direction[y * src.width + x] = std::atan2((double)sy, (double)sx);
        }
    }
    return r;
}

// ---------------- Step 3: Non-maximum suppression ----------------
std::vector<double> nonMaxSuppress(const GradResult &g) {
    std::vector<double> out(g.magnitude.size(), 0.0);
    int w = g.width, h = g.height;

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            double angle = g.direction[y * w + x] * 180.0 / M_PI;
            if (angle < 0) angle += 180.0;

            double mag = g.magnitude[y * w + x];
            double m1, m2;

            if ((angle >= 0 && angle < 22.5) || (angle >= 157.5 && angle <= 180)) {
                m1 = g.magnitude[y * w + (x - 1)];
                m2 = g.magnitude[y * w + (x + 1)];
            } else if (angle >= 22.5 && angle < 67.5) {
                m1 = g.magnitude[(y - 1) * w + (x + 1)];
                m2 = g.magnitude[(y + 1) * w + (x - 1)];
            } else if (angle >= 67.5 && angle < 112.5) {
                m1 = g.magnitude[(y - 1) * w + x];
                m2 = g.magnitude[(y + 1) * w + x];
            } else {
                m1 = g.magnitude[(y - 1) * w + (x - 1)];
                m2 = g.magnitude[(y + 1) * w + (x + 1)];
            }

            out[y * w + x] = (mag >= m1 && mag >= m2) ? mag : 0.0;
        }
    }
    return out;
}

// ---------------- Step 4: Double threshold + hysteresis ----------------
Image hysteresis(const std::vector<double> &nms, int w, int h, int lowT, int highT) {
    Image edges;
    edges.width = w;
    edges.height = h;
    edges.data.assign((size_t)w * h, 0);

    std::vector<unsigned char> state((size_t)w * h, 0);
    for (size_t i = 0; i < nms.size(); i++) {
        if (nms[i] >= highT) state[i] = 2;
        else if (nms[i] >= lowT) state[i] = 1;
    }

    std::vector<unsigned char> visited((size_t)w * h, 0);
    std::queue<int> q;
    for (int i = 0; i < w * h; i++) {
        if (state[i] == 2) {
            q.push(i);
            visited[i] = 1;
            edges.data[i] = 255;
        }
    }

    int dx[8] = {-1,0,1,-1,1,-1,0,1};
    int dy[8] = {-1,-1,-1,0,0,1,1,1};

    while (!q.empty()) {
        int idx = q.front(); q.pop();
        int x = idx % w, y = idx / w;
        for (int k = 0; k < 8; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
            int nidx = ny * w + nx;
            if (!visited[nidx] && (state[nidx] == 1 || state[nidx] == 2)) {
                visited[nidx] = 1;
                edges.data[nidx] = 255;
                q.push(nidx);
            }
        }
    }
    return edges;
}

Image cannyEdgeDetect(const Image &src, int lowT, int highT) {
    Image blurred = gaussianBlur(src);
    GradResult grad = sobel(blurred);
    std::vector<double> nms = nonMaxSuppress(grad);
    return hysteresis(nms, src.width, src.height, lowT, highT);
}

// ---------------- Step 5: Corner detection (direction-change based) ----------------
// For each edge pixel, look at all edge pixels within CORNER_WINDOW radius.
// Find the two "arm" points: the farthest edge pixel from the center (A),
// and the edge pixel farthest from A within the window (B). These approximate
// the two edge directions passing through the center pixel. If the angle
// between vectors (center->A) and (center->B) is well short of 180 degrees
// (i.e. not a straight line), and both arms are long enough to be reliable,
// the pixel is flagged as a corner candidate.
std::vector<Point2> detectCornersRaw(const Image &edges) {
    std::vector<Point2> candidates;
    int w = edges.width, h = edges.height;
    int R = CORNER_WINDOW;

    for (int y = R; y < h - R; y++) {
        for (int x = R; x < w - R; x++) {
            if (edges.at(x, y) == 0) continue;

            // Gather edge pixels in the window
            Point2 farthestA{x, y};
            double bestDistA = -1;
            for (int dy = -R; dy <= R; dy++) {
                for (int dx = -R; dx <= R; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    if (edges.at(x + dx, y + dy) > 0) {
                        double d = std::sqrt((double)(dx*dx + dy*dy));
                        if (d > bestDistA) {
                            bestDistA = d;
                            farthestA = {x + dx, y + dy};
                        }
                    }
                }
            }
            if (bestDistA < CORNER_MIN_ARM_LEN) continue; // not enough edge around it

            // Find point farthest from A within the window (the "other arm")
            Point2 farthestB{x, y};
            double bestDistB = -1;
            for (int dy = -R; dy <= R; dy++) {
                for (int dx = -R; dx <= R; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    if (edges.at(x + dx, y + dy) > 0) {
                        double ddx = (x + dx) - farthestA.x;
                        double ddy = (y + dy) - farthestA.y;
                        double d = std::sqrt(ddx*ddx + ddy*ddy);
                        if (d > bestDistB) {
                            bestDistB = d;
                            farthestB = {x + dx, y + dy};
                        }
                    }
                }
            }

            // Vector from center to each arm
            double v1x = farthestA.x - x, v1y = farthestA.y - y;
            double v2x = farthestB.x - x, v2y = farthestB.y - y;
            double len1 = std::sqrt(v1x*v1x + v1y*v1y);
            double len2 = std::sqrt(v2x*v2x + v2y*v2y);
            if (len1 < CORNER_MIN_ARM_LEN || len2 < CORNER_MIN_ARM_LEN) continue;

            double dot = (v1x*v2x + v1y*v2y) / (len1 * len2);
            dot = std::max(-1.0, std::min(1.0, dot)); // clamp for safety
            double angleDeg = std::acos(dot) * 180.0 / M_PI;

            // A straight edge gives an angle near 180 degrees (arms point opposite ways).
            // A real corner gives a noticeably smaller angle.
            if (angleDeg < CORNER_MAX_ANGLE) {
                candidates.push_back({x, y});
            }
        }
    }
    return candidates;
}

// ---------------- Step 5b: Cluster nearby corner candidates into single points ----------------
std::vector<Point2> clusterCorners(const std::vector<Point2> &candidates, int radius) {
    std::vector<bool> used(candidates.size(), false);
    std::vector<Point2> clustered;

    for (size_t i = 0; i < candidates.size(); i++) {
        if (used[i]) continue;

        long sumX = candidates[i].x, sumY = candidates[i].y;
        int count = 1;
        used[i] = true;

        for (size_t j = i + 1; j < candidates.size(); j++) {
            if (used[j]) continue;
            int dx = candidates[j].x - candidates[i].x;
            int dy = candidates[j].y - candidates[i].y;
            if (dx*dx + dy*dy <= radius*radius) {
                sumX += candidates[j].x;
                sumY += candidates[j].y;
                count++;
                used[j] = true;
            }
        }

        clustered.push_back({ (int)(sumX / count), (int)(sumY / count) });
    }
    return clustered;
}

std::vector<Point2> detectCorners(const Image &edges) {
    std::vector<Point2> raw = detectCornersRaw(edges);
    return clusterCorners(raw, CLUSTER_RADIUS);
}

// ---------------- Step 6: Diagonal distance ----------------
double computeDiagonalDistance(const std::vector<Point2> &corners, Point2 &tl, Point2 &br) {
    if (corners.empty()) return -1.0;

    int minX = corners[0].x, maxX = corners[0].x;
    int minY = corners[0].y, maxY = corners[0].y;
    for (const auto &p : corners) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
    }
    tl = {minX, minY};
    br = {maxX, maxY};

    double dx = br.x - tl.x;
    double dy = br.y - tl.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ---------------- Step 7: Depth classification ----------------
std::string classifyDepth(double distance) {
    if (distance < 0) return "No object detected";
    return (distance > DEPTH_THRESHOLD) ? "NEAR (large corner distance)"
                                         : "FAR (small corner distance)";
}

int main(int argc, char **argv) {
    Image src;

    if (argc < 2) {
        generateTestSquare(src);
    } else {
        std::string inputPath = argv[1];
        try {
            std::string ext;
            auto dotPos = inputPath.find_last_of('.');
            if (dotPos != std::string::npos) ext = inputPath.substr(dotPos + 1);
            for (auto &c : ext) c = static_cast<char>(tolower(c));

            if (ext == "pgm") {
                src = readPGM(inputPath);
            } else {
                src = readBMP(inputPath);
            }
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    try {
        std::cout << "Loaded image: " << src.width << "x" << src.height << "\n";
        writeGrayBMP("original_output.bmp", src);

        Image edges = cannyEdgeDetect(src, CANNY_LOW, CANNY_HIGH);
        writeGrayBMP("edges_output.bmp", edges);

        std::vector<Point2> corners = detectCorners(edges);
        std::cout << "Detected " << corners.size() << " corner points (after clustering).\n";
        for (const auto &p : corners) {
            std::cout << "  corner at (" << p.x << ", " << p.y << ")\n";
        }

        Point2 tl{0,0}, br{0,0};
        double distance = computeDiagonalDistance(corners, tl, br);
        std::cout << "Top-left corner: (" << tl.x << ", " << tl.y << ")\n";
        std::cout << "Bottom-right corner: (" << br.x << ", " << br.y << ")\n";
        std::cout << "Diagonal distance: " << distance << " px\n";
        std::cout << "Depth classification: " << classifyDepth(distance) << "\n";

        // Visualization: edges in white, corners as larger red markers, bounding box in green
        std::vector<unsigned char> rgb((size_t)edges.width * edges.height * 3, 0);
        for (int y = 0; y < edges.height; y++) {
            for (int x = 0; x < edges.width; x++) {
                unsigned char v = edges.at(x, y);
                size_t i = ((size_t)y * edges.width + x) * 3;
                rgb[i] = rgb[i+1] = rgb[i+2] = v;
            }
        }
        // Draw each corner as a filled 5x5 red marker so it's clearly visible
        for (const auto &p : corners) {
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int px = p.x + dx, py = p.y + dy;
                    if (px < 0 || px >= edges.width || py < 0 || py >= edges.height) continue;
                    size_t i = ((size_t)py * edges.width + px) * 3;
                    rgb[i] = 255; rgb[i+1] = 0; rgb[i+2] = 0;
                }
            }
        }
        if (distance >= 0) {
            for (int x = tl.x; x <= br.x; x++) {
                for (int y : {tl.y, br.y}) {
                    size_t i = ((size_t)y * edges.width + x) * 3;
                    rgb[i] = 0; rgb[i+1] = 255; rgb[i+2] = 0;
                }
            }
            for (int y = tl.y; y <= br.y; y++) {
                for (int x : {tl.x, br.x}) {
                    size_t i = ((size_t)y * edges.width + x) * 3;
                    rgb[i] = 0; rgb[i+1] = 255; rgb[i+2] = 0;
                }
            }
        }
        writeBMP24("corners_output.bmp", edges.width, edges.height, rgb);
        std::cout << "Saved original_output.bmp, edges_output.bmp, corners_output.bmp\n";

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}


//distance = (real_width × focal_length) / pixel_width