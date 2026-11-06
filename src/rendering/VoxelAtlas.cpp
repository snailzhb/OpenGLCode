#include "rendering/VoxelAtlas.h"

#include "root_directory.h"

#include <glad/glad.h>

#include <glm/common.hpp>

#include <stb_image.h>

#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

inline float hash2(float x, float y) {
    const float t = std::sin(x * 127.1f + y * 311.7f) * 43758.5453f;
    return t - std::floor(t);
}

inline float hash3(float x, float y, float z) {
    return hash2(hash2(x, y) * 19.7f + z, x * 0.37f + y);
}

inline void setPx(std::vector<unsigned char>& rgba, int ax, int ay, int w, unsigned char r, unsigned char g,
                  unsigned char b, unsigned char a = 255) {
    const size_t i = (static_cast<size_t>(ay) * static_cast<size_t>(w) + static_cast<size_t>(ax)) * 4u;
    rgba[i] = r;
    rgba[i + 1] = g;
    rgba[i + 2] = b;
    rgba[i + 3] = a;
}

/** Voronoi 到最近 cell 边界的近似距离（小=缝/裂纹） */
float voronoiEdgeDist(float x, float y) {
    const float ix = std::floor(x);
    const float iy = std::floor(y);
    float md = 10.0f;
    float md2 = 10.0f;
    for (int j = -1; j <= 1; ++j) {
        for (int i = -1; i <= 1; ++i) {
            const float cx = ix + static_cast<float>(i) + hash2(ix + static_cast<float>(i) + 13.0f, iy + static_cast<float>(j) + 17.0f);
            const float cy = iy + static_cast<float>(j) + hash2(ix + static_cast<float>(i) + 31.0f, iy + static_cast<float>(j) + 41.0f);
            const float d = std::hypot(x - cx, y - cy);
            if (d < md) {
                md2 = md;
                md = d;
            } else if (d < md2)
                md2 = d;
        }
    }
    return md2 - md;
}

void fillGrass(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        const float vf = static_cast<float>(y) / static_cast<float>(th - 1);
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float n = 0.72f + 0.28f * hash2(xf * 0.31f, yf * 0.29f);
            const float n2 = 0.85f + 0.15f * hash2(xf * 0.11f, yf * 0.13f);
            const float blade = 0.5f + 0.5f * std::sin(xf * 1.15f + std::sin(yf * 0.09f) * 6.0f);
            const float blade2 = 0.5f + 0.5f * std::sin(yf * 1.05f + std::cos(xf * 0.07f) * 4.0f);
            const float tuft = std::pow(std::max(blade, blade2), 2.2f) * 0.22f;
            const float greenTop = std::pow(1.0f - vf, 0.32f);
            const float dirtMix = std::pow(vf, 1.75f) * 0.94f;
            const float flower = (hash3(xf * 0.4f, yf * 0.4f, 2.0f) > 0.965f && vf < 0.35f) ? 1.0f : 0.0f;
            float r = (0.20f * greenTop + 0.40f * dirtMix) * n * n2 + tuft * 0.08f;
            float g = (0.52f * greenTop + 0.30f * dirtMix) * n * n2 + tuft * 0.18f;
            float b = (0.14f * greenTop + 0.12f * dirtMix) * n * n2 + tuft * 0.04f;
            r += flower * 0.22f;
            g += flower * 0.20f;
            b += flower * 0.04f;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillDirt(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float strata = 0.5f + 0.5f * std::sin(yf * 0.42f + std::sin(xf * 0.08f) * 1.8f);
            const float n = 0.78f + 0.22f * hash2(xf * 0.19f, yf * 0.21f);
            const float peb = std::pow(hash2(xf * 0.55f, yf * 0.52f), 5.0f) * 0.45f;
            const float lime = (hash2(xf * 0.73f, yf * 0.71f) > 0.94f) ? 0.12f : 0.0f;
            const float s = 0.88f + 0.12f * strata;
            float r = (0.46f * n * s + peb * 0.08f) + lime;
            float g = (0.30f * n * s + peb * 0.05f) + lime;
            float b = (0.15f * n * s + peb * 0.03f) + lime * 0.85f;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillStone(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = (static_cast<float>(tx + x)) * 0.11f;
            const float yf = (static_cast<float>(ty0 + y)) * 0.11f;
            const float edge = voronoiEdgeDist(xf, yf);
            const float crack = std::pow(std::max(0.0f, 0.14f - edge) / 0.14f, 1.4f);
            const float n = hash2(xf * 2.1f, yf * 2.3f);
            const float speck = std::pow(hash2(static_cast<float>(y) * 3.1f, static_cast<float>(x) * 2.7f), 6.0f) * 0.4f;
            float v = 0.36f + 0.42f * n + speck * 0.35f;
            v *= 1.0f - crack * 0.55f;
            v = std::min(1.0f, v);
            const float moss = (hash2(xf * 3.0f, yf * 3.0f) > 0.91f && crack < 0.2f) ? 0.06f : 0.0f;
            float r = v + moss * 0.05f;
            float g = v * 0.98f + moss * 0.12f;
            float b = std::min(1.0f, v * 0.96f + speck * 0.06f + moss * 0.04f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r * 255.0f),
                  static_cast<unsigned char>(g * 255.0f), static_cast<unsigned char>(b * 255.0f));
        }
    }
}

void fillSand(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float ripple = 0.5f + 0.5f * std::sin(xf * 0.55f + std::sin(yf * 0.31f) * 2.2f);
            const float n = 0.74f + 0.26f * hash2(xf * 0.15f, yf * 0.17f);
            const float grain = 0.92f + 0.08f * hash2(xf * 0.9f, yf * 0.88f);
            const float w = ripple * grain;
            float r = (0.86f * n + 0.06f * w) * 0.99f;
            float g = (0.74f * n + 0.04f * w) * 0.97f;
            float b = (0.44f * n + 0.03f * w) * 0.94f;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillWater(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float n = 0.65f + 0.35f * hash2(static_cast<float>(tx + x) * 0.25f, static_cast<float>(ty0 + y) * 0.27f);
            const float caust = 0.5f + 0.5f * std::sin(static_cast<float>(x + y) * 0.35f);
            float r = (0.10f + 0.04f * caust) * n;
            float g = (0.36f + 0.08f * caust) * n;
            float b = (0.68f + 0.06f * caust) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b), 255);
        }
    }
}

void fillWood(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const float cx1 = static_cast<float>(th) * 0.5f;
    const float cy1 = static_cast<float>(th) * 0.22f;
    const float cx2 = static_cast<float>(th) * 0.375f;
    const float cy2 = static_cast<float>(th) * 0.31f;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float ring = 0.5f + 0.5f * std::sin(xf * 0.88f + std::sin(yf * 0.11f) * 2.2f);
            const float n = 0.78f + 0.22f * hash2(yf * 0.38f, xf * 0.06f);
            const float dx = xf - cx1;
            const float dy = yf - cy1;
            const float knot = std::exp(-(dx * dx + dy * dy) / (static_cast<float>(th * th) * 0.018f)) * 0.55f;
            const float dx2 = xf - cx2;
            const float dy2 = yf - cy2;
            const float knot2 = std::exp(-(dx2 * dx2 + dy2 * dy2) / (static_cast<float>(th * th) * 0.022f)) * 0.45f;
            float r = (0.34f + 0.15f * ring) * n * (1.0f - knot * 0.55f - knot2 * 0.45f);
            float g = (0.20f + 0.09f * ring) * n * (1.0f - knot * 0.5f - knot2 * 0.42f);
            float b = (0.08f + 0.04f * ring) * n * (1.0f - knot * 0.45f - knot2 * 0.4f);
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillCobble(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const int cell = std::max(14, (th * 14 + 32) / 64);
    const int half = cell / 2;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const int bx = (x / cell) % 5;
            const int by = (y / cell) % 5;
            const float lx = (static_cast<float>(x % cell) - static_cast<float>(half)) / static_cast<float>(half);
            const float ly = (static_cast<float>(y % cell) - static_cast<float>(half)) / static_cast<float>(half);
            const float rr = std::sqrt(lx * lx + ly * ly) + 0.08f * std::sin(lx * 9.0f) * std::sin(ly * 9.0f);
            const float stoneBody = glm::smoothstep(0.92f, 0.55f, rr);
            const float mortar = 1.0f - stoneBody;
            const float cellVar = 0.82f + 0.18f * hash2(static_cast<float>(bx * 19 + by * 7), 2.3f);
            const float n = 0.7f + 0.3f * hash2(static_cast<float>(tx + x) * 0.13f, static_cast<float>(ty0 + y) * 0.15f);
            float v = (0.38f + 0.22f * n) * cellVar * stoneBody + mortar * 0.22f;
            v = std::min(1.0f, v);
            const unsigned char c = static_cast<unsigned char>(v * 255.0f);
            const unsigned char cg = static_cast<unsigned char>(std::min(255.0f, (v * 0.97f - mortar * 0.08f) * 255.0f));
            const unsigned char cb = static_cast<unsigned char>(std::min(255.0f, (v * 1.02f - mortar * 0.05f) * 255.0f));
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, c, cg, cb);
        }
    }
}

void fillGlass(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const int m = std::max(24, th / 2);
    const int fthick = std::max(2, th / 32);
    const int m1 = m - fthick;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float n = 0.9f + 0.1f * hash2(static_cast<float>(tx + x) * 0.5f, static_cast<float>(ty0 + y) * 0.5f);
            const int fx = (x + y) % m;
            const int fy = (x - y + m * 4) % m;
            const float frame = (fx < fthick || fy < fthick || fx > m1 || fy > m1) ? 0.75f : 1.0f;
            float r = 0.72f * n * frame;
            float g = 0.84f * n * frame;
            float b = 0.92f * n * frame;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillGlassLight(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float n = 0.92f + 0.08f * hash2(static_cast<float>(tx + x) * 0.31f, static_cast<float>(ty0 + y) * 0.29f);
            const float frost = 0.15f * std::sin(static_cast<float>(x + y) * 0.11f);
            float r = (0.88f + frost) * n;
            float g = (0.93f + frost * 0.4f) * n;
            float b = (0.98f + frost * 0.2f) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillGlassDark(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float n = 0.7f + 0.3f * hash2(static_cast<float>(tx + x) * 0.21f, static_cast<float>(ty0 + y) * 0.23f);
            const float seam = 0.5f + 0.5f * std::sin(static_cast<float>(x + y) * 0.09f);
            float r = (0.06f + 0.04f * seam) * n;
            float g = (0.08f + 0.05f * seam) * n;
            float b = (0.14f + 0.08f * seam) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillBrick(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const int bh = std::max(10, th / 12);
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const int bx = x / bh;
            const int by = y / bh;
            const float mortar = ((x % bh < 2) || (y % bh < 2)) ? 0.28f : 0.0f;
            const float n = 0.72f + 0.28f * hash2(xf * 0.21f, yf * 0.19f);
            float r = (0.62f + 0.12f * hash2(static_cast<float>(bx * 17 + by), 3.1f)) * n * (1.0f - mortar) + mortar * 0.35f;
            float g = (0.22f + 0.06f * n) * (1.0f - mortar * 0.6f);
            float b = (0.14f + 0.04f * n) * (1.0f - mortar * 0.55f);
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillHayThatch(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float straw = 0.5f + 0.5f * std::sin(xf * 0.95f + std::sin(yf * 0.11f) * 3.0f);
            const float n = 0.78f + 0.22f * hash2(xf * 0.31f, yf * 0.29f);
            float r = (0.72f + 0.12f * straw) * n;
            float g = (0.58f + 0.1f * straw) * n;
            float b = (0.12f + 0.04f * straw) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillPathStone(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float peb = std::pow(hash2(xf * 0.62f, yf * 0.58f), 3.2f);
            const float n = 0.75f + 0.25f * hash2(xf * 0.17f, yf * 0.19f);
            float v = (0.42f + 0.18f * n + peb * 0.25f);
            v = std::min(1.0f, v);
            const unsigned char c = static_cast<unsigned char>(v * 255.0f);
            const unsigned char cg = static_cast<unsigned char>(std::min(255.0f, (v * 0.99f) * 255.0f));
            const unsigned char cb = static_cast<unsigned char>(std::min(255.0f, (v * 1.02f) * 255.0f));
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, c, cg, cb);
        }
    }
}

void fillMossBrick(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float n = 0.7f + 0.3f * hash2(xf * 0.19f, yf * 0.21f);
            const float moss = (hash2(xf * 0.41f, yf * 0.43f) > 0.82f) ? 0.14f : 0.0f;
            float r = (0.28f * n + moss * 0.05f);
            float g = (0.32f * n + moss * 0.16f);
            float b = (0.26f * n + moss * 0.04f);
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillSlate(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float layer = 0.5f + 0.5f * std::sin(yf * 0.08f + std::sin(xf * 0.05f) * 1.6f);
            const float n = 0.68f + 0.32f * hash2(xf * 0.14f, yf * 0.16f);
            float r = (0.22f + 0.06f * layer) * n;
            float g = (0.26f + 0.07f * layer) * n;
            float b = (0.34f + 0.10f * layer) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillPlank(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const int plankH = std::max(8, th / 14);
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const int row = y / plankH;
            const float seam = (y % plankH < 2) ? 0.35f : 0.0f;
            const float grain = 0.5f + 0.5f * std::sin(xf * 0.72f + static_cast<float>(row) * 0.41f);
            const float n = 0.76f + 0.24f * hash2(xf * 0.09f, yf * 0.11f);
            float r = (0.48f + 0.14f * grain) * n * (1.0f - seam) + seam * 0.28f;
            float g = (0.32f + 0.08f * grain) * n * (1.0f - seam * 0.7f);
            float b = (0.14f + 0.04f * grain) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillMarble(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = (static_cast<float>(tx + x)) * 0.07f;
            const float yf = (static_cast<float>(ty0 + y)) * 0.07f;
            const float vein = 0.5f + 0.5f * std::sin(xf * 2.1f + std::sin(yf * 1.7f) * 2.4f);
            const float n = 0.84f + 0.16f * hash2(xf * 3.2f, yf * 3.1f);
            const float v = (0.82f + 0.12f * vein) * n;
            float r = v;
            float g = v * 0.98f;
            float b = v * 0.96f;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillIron(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float brush = 0.5f + 0.5f * std::sin(xf * 0.22f + yf * 0.04f);
            const float n = 0.72f + 0.28f * hash2(xf * 0.31f, yf * 0.29f);
            const float spec = std::pow(brush, 3.0f) * 0.18f;
            float v = (0.42f + 0.22f * n + spec);
            v = std::min(1.0f, v);
            const unsigned char c = static_cast<unsigned char>(v * 255.0f);
            const unsigned char cb = static_cast<unsigned char>(std::min(255.0f, (v * 1.04f) * 255.0f));
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, c, c, cb);
        }
    }
}

void fillMossStone(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float edge = voronoiEdgeDist(xf * 0.09f, yf * 0.09f);
            const float crack = std::pow(std::max(0.0f, 0.12f - edge) / 0.12f, 1.2f);
            const float n = 0.65f + 0.35f * hash2(xf * 0.17f, yf * 0.19f);
            const float moss = (hash2(xf * 0.33f, yf * 0.37f) > 0.78f) ? 0.22f : 0.0f;
            float v = (0.34f + 0.18f * n) * (1.0f - crack * 0.4f);
            float r = v + moss * 0.04f;
            float g = v * 0.96f + moss * 0.18f;
            float b = v * 0.92f + moss * 0.05f;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillFlowerGrass(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        const float vf = static_cast<float>(y) / static_cast<float>(th - 1);
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float n = 0.74f + 0.26f * hash2(xf * 0.27f, yf * 0.25f);
            const float tuft = 0.5f + 0.5f * std::sin(xf * 1.05f + yf * 0.13f);
            const float flower = hash3(xf * 0.35f, yf * 0.35f, 1.7f);
            float r = (0.18f + 0.42f * (1.0f - vf)) * n + tuft * 0.06f;
            float g = (0.48f + 0.38f * (1.0f - vf)) * n + tuft * 0.14f;
            float b = (0.10f + 0.08f * vf) * n;
            if (flower > 0.968f && vf < 0.45f) {
                r += 0.35f;
                g += 0.12f;
                b += 0.08f;
            } else if (flower > 0.982f && vf < 0.5f) {
                r += 0.15f;
                g += 0.28f;
                b += 0.42f;
            }
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillShingle(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const int sh = std::max(9, th / 13);
    const int sw = std::max(14, th / 9);
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const int row = y / sh;
            const int col = (x + (row % 2) * (sw / 2)) / sw;
            const float lx = static_cast<float>((x + (row % 2) * (sw / 2)) % sw) / static_cast<float>(sw);
            const float ly = static_cast<float>(y % sh) / static_cast<float>(sh);
            const float arc = std::sin(lx * 3.14159265f) * (1.0f - ly * 0.35f);
            const float n = 0.78f + 0.22f * hash2(static_cast<float>(col * 11 + row * 7), 2.1f);
            float r = (0.52f + 0.14f * arc) * n;
            float g = (0.24f + 0.06f * arc) * n;
            float b = (0.12f + 0.03f * arc) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillDarkWood(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const float ring = 0.5f + 0.5f * std::sin(xf * 0.62f + std::sin(yf * 0.14f) * 2.0f);
            const float n = 0.72f + 0.28f * hash2(yf * 0.33f, xf * 0.05f);
            float r = (0.22f + 0.10f * ring) * n;
            float g = (0.14f + 0.06f * ring) * n;
            float b = (0.06f + 0.03f * ring) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillTerracotta(std::vector<unsigned char>& rgba, int tx, int ty0, int /*tw*/, int th) {
    const int rows = 6;
    const int rh = th / rows;
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float xf = static_cast<float>(tx + x);
            const float yf = static_cast<float>(ty0 + y);
            const int ri = y / std::max(1, rh);
            const float wave = 0.5f + 0.5f * std::sin(xf * 0.14f + static_cast<float>(ri) * 0.9f);
            const float n = 0.82f + 0.18f * hash2(xf * 0.11f, yf * 0.13f);
            float r = (0.62f + 0.12f * wave) * n;
            float g = (0.28f + 0.06f * wave) * n;
            float b = (0.12f + 0.03f * wave) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

void fillLampTile(std::vector<unsigned char>& rgba, int tx, int ty0, int th, float br, float bg, float bb) {
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < th; ++x) {
            const float n = 0.88f + 0.12f * hash2(static_cast<float>(tx + x) * 0.17f, static_cast<float>(ty0 + y) * 0.19f);
            const float core = std::pow(std::max(0.0f, 1.0f - std::hypot(static_cast<float>(x - th / 2) / static_cast<float>(th * 0.48f),
                                                                        static_cast<float>(y - th / 2) / static_cast<float>(th * 0.48f))),
                                          2.2f);
            float r = (br * 0.55f + br * 0.45f * core) * n;
            float g = (bg * 0.55f + bg * 0.45f * core) * n;
            float b = (bb * 0.55f + bb * 0.45f * core) * n;
            r = std::min(255.0f, r * 255.0f);
            g = std::min(255.0f, g * 255.0f);
            b = std::min(255.0f, b * 255.0f);
            setPx(rgba, tx + x, ty0 + y, VoxelAtlas::kAtlasW, static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                  static_cast<unsigned char>(b));
        }
    }
}

bool buildProceduralRgba(std::vector<unsigned char>& rgba) {
    const int W = VoxelAtlas::kAtlasW;
    const int H = VoxelAtlas::kAtlasH;
    const int T = VoxelAtlas::kTilePx;
    rgba.assign(static_cast<size_t>(W * H * 4), 255);
    for (int tile = 0; tile < VoxelAtlas::kTiles; ++tile) {
        const int tx = tile * T;
        const int ty0 = 0;
        switch (tile + 1) {
        case 1:
            fillGrass(rgba, tx, ty0, T, H);
            break;
        case 2:
            fillDirt(rgba, tx, ty0, T, H);
            break;
        case 3:
            fillStone(rgba, tx, ty0, T, H);
            break;
        case 4:
            fillSand(rgba, tx, ty0, T, H);
            break;
        case 5:
            fillWater(rgba, tx, ty0, T, H);
            break;
        case 6:
            fillWood(rgba, tx, ty0, T, H);
            break;
        case 7:
            fillCobble(rgba, tx, ty0, T, H);
            break;
        case 8:
            fillGlass(rgba, tx, ty0, T, H);
            break;
        case 9:
            fillGlassLight(rgba, tx, ty0, T, H);
            break;
        case 10:
            fillGlassDark(rgba, tx, ty0, T, H);
            break;
        case 11:
            fillLampTile(rgba, tx, ty0, H, 0.95f, 0.97f, 1.0f);
            break;
        case 12:
            fillLampTile(rgba, tx, ty0, H, 1.0f, 0.58f, 0.22f);
            break;
        case 13:
            fillLampTile(rgba, tx, ty0, H, 0.22f, 0.92f, 0.98f);
            break;
        case 14:
            fillLampTile(rgba, tx, ty0, H, 0.95f, 0.28f, 1.0f);
            break;
        case 15:
            fillBrick(rgba, tx, ty0, T, H);
            break;
        case 16:
            fillHayThatch(rgba, tx, ty0, T, H);
            break;
        case 17:
            fillPathStone(rgba, tx, ty0, T, H);
            break;
        case 18:
            fillMossBrick(rgba, tx, ty0, T, H);
            break;
        case 19:
            fillTerracotta(rgba, tx, ty0, T, H);
            break;
        case 20:
            fillSlate(rgba, tx, ty0, T, H);
            break;
        case 21:
            fillPlank(rgba, tx, ty0, T, H);
            break;
        case 22:
            fillMarble(rgba, tx, ty0, T, H);
            break;
        case 23:
            fillIron(rgba, tx, ty0, T, H);
            break;
        case 24:
            fillMossStone(rgba, tx, ty0, T, H);
            break;
        case 25:
            fillFlowerGrass(rgba, tx, ty0, T, H);
            break;
        case 26:
            fillShingle(rgba, tx, ty0, T, H);
            break;
        case 27:
            fillDarkWood(rgba, tx, ty0, T, H);
            break;
        case 28:
            fillMossStone(rgba, tx, ty0, T, H);
            break;
        default:
            fillStone(rgba, tx, ty0, T, H);
            break;
        }
    }
    return true;
}

void upscaleNearest2x(const std::vector<unsigned char>& src, int sw, int sh, std::vector<unsigned char>& dst, int dw,
                      int dh) {
    dst.resize(static_cast<size_t>(dw * dh * 4));
    for (int y = 0; y < dh; ++y) {
        for (int x = 0; x < dw; ++x) {
            const int sx = std::min(sw - 1, x / 2);
            const int sy = std::min(sh - 1, y / 2);
            const size_t si = (static_cast<size_t>(sy) * static_cast<size_t>(sw) + static_cast<size_t>(sx)) * 4u;
            const size_t di = (static_cast<size_t>(y) * static_cast<size_t>(dw) + static_cast<size_t>(x)) * 4u;
            dst[di] = src[si];
            dst[di + 1] = src[si + 1];
            dst[di + 2] = src[si + 2];
            dst[di + 3] = src[si + 3];
        }
    }
}

void buildNormalsFromDiffuse(const std::vector<unsigned char>& rgba, int w, int h, std::vector<unsigned char>& outN) {
    outN.assign(static_cast<size_t>(w * h * 4), 255);
    auto lum = [&](int x, int y) -> float {
        x = std::max(0, std::min(w - 1, x));
        y = std::max(0, std::min(h - 1, y));
        const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4u;
        return (0.299f * static_cast<float>(rgba[i]) + 0.587f * static_cast<float>(rgba[i + 1]) +
                0.114f * static_cast<float>(rgba[i + 2])) *
               (1.0f / 255.0f);
    };
    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            const float dx = lum(x + 1, y) - lum(x - 1, y);
            const float dy = lum(x, y + 1) - lum(x, y - 1);
            float nx = -dx * 2.35f;
            float ny = 1.0f;
            float nz = -dy * 2.35f;
            const float inv = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            nx *= inv;
            ny *= inv;
            nz *= inv;
            const size_t o = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4u;
            outN[o] = static_cast<unsigned char>((nx * 0.5f + 0.5f) * 255.0f);
            outN[o + 1] = static_cast<unsigned char>((ny * 0.5f + 0.5f) * 255.0f);
            outN[o + 2] = static_cast<unsigned char>((nz * 0.5f + 0.5f) * 255.0f);
            outN[o + 3] = 255;
        }
    }
}

} // namespace

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

static void applyAtlasSamplingParams() {
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    if (maxAniso > 1.0f)
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, maxAniso));
}

bool VoxelAtlas::init() {
    shutdown();
    std::vector<unsigned char> rgba;
    int iw = 0;
    int ih = 0;
    int comp = 0;
    const std::string path = std::string(logl_root) + "/resources/textures/voxel_atlas.png";
    if (unsigned char* data = stbi_load(path.c_str(), &iw, &ih, &comp, 4)) {
        if (iw == kAtlasW && ih == kAtlasH) {
            rgba.assign(data, data + static_cast<size_t>(iw * ih * 4));
            stbi_image_free(data);
        } else if (iw == kAtlasW / 2 && ih == kAtlasH / 2) {
            std::vector<unsigned char> tmp(data, data + static_cast<size_t>(iw * ih * 4));
            stbi_image_free(data);
            upscaleNearest2x(tmp, iw, ih, rgba, kAtlasW, kAtlasH);
        } else {
            stbi_image_free(data);
            std::cerr << "VoxelAtlas: " << path << " size " << iw << "x" << ih << " expected " << kAtlasW << "x" << kAtlasH
                      << " (or half for upscale), using procedural\n";
            buildProceduralRgba(rgba);
        }
    } else {
        buildProceduralRgba(rgba);
    }

    std::vector<unsigned char> normRgba;
    const std::string npath = std::string(logl_root) + "/resources/textures/voxel_atlas_n.png";
    int nw = 0;
    int nh = 0;
    int nc = 0;
    if (unsigned char* nd = stbi_load(npath.c_str(), &nw, &nh, &nc, 4)) {
        if (nw == kAtlasW && nh == kAtlasH) {
            normRgba.assign(nd, nd + static_cast<size_t>(nw * nh * 4));
            stbi_image_free(nd);
        } else {
            stbi_image_free(nd);
            std::cerr << "VoxelAtlas: " << npath << " size mismatch, generating normals from diffuse\n";
            buildNormalsFromDiffuse(rgba, kAtlasW, kAtlasH, normRgba);
        }
    } else {
        buildNormalsFromDiffuse(rgba, kAtlasW, kAtlasH, normRgba);
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlasW, kAtlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.55f);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    applyAtlasSamplingParams();
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenTextures(1, &normalTexture);
    glBindTexture(GL_TEXTURE_2D, normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlasW, kAtlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE, normRgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    applyAtlasSamplingParams();
    glBindTexture(GL_TEXTURE_2D, 0);

    return texture != 0 && normalTexture != 0;
}

void VoxelAtlas::shutdown() {
    if (texture) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
    if (normalTexture) {
        glDeleteTextures(1, &normalTexture);
        normalTexture = 0;
    }
}
