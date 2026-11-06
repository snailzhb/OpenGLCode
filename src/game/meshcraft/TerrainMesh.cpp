#include "game/meshcraft/TerrainMesh.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace meshcraft {

namespace {

float hash2(float x, float z) {
    return std::fmod(std::sin(x * 127.1f + z * 311.7f) * 43758.5453f, 1.0f);
}

float noise2(float x, float z) {
    const int xi = static_cast<int>(std::floor(x));
    const int zi = static_cast<int>(std::floor(z));
    const float fx = x - static_cast<float>(xi);
    const float fz = z - static_cast<float>(zi);
    const float ux = fx * fx * (3.0f - 2.0f * fx);
    const float uz = fz * fz * (3.0f - 2.0f * fz);
    const float a = hash2(static_cast<float>(xi), static_cast<float>(zi));
    const float b = hash2(static_cast<float>(xi + 1), static_cast<float>(zi));
    const float c = hash2(static_cast<float>(xi), static_cast<float>(zi + 1));
    const float d = hash2(static_cast<float>(xi + 1), static_cast<float>(zi + 1));
    return glm::mix(glm::mix(a, b, ux), glm::mix(c, d, ux), uz);
}

float fbm(float x, float z) {
    float v = 0.0f;
    float a = 0.5f;
    float sx = x;
    float sz = z;
    for (int i = 0; i < 5; ++i) {
        v += a * noise2(sx, sz);
        sx *= 2.02f;
        sz *= 2.02f;
        a *= 0.5f;
    }
    return v;
}

float distToSegment2D(float px, float pz, float ax, float az, float bx, float bz) {
    const float abx = bx - ax;
    const float abz = bz - az;
    const float apx = px - ax;
    const float apz = pz - az;
    const float ab2 = abx * abx + abz * abz;
    float t = ab2 > 1e-6f ? (apx * abx + apz * abz) / ab2 : 0.0f;
    t = glm::clamp(t, 0.0f, 1.0f);
    const float cx = ax + abx * t;
    const float cz = az + abz * t;
    const float dx = px - cx;
    const float dz = pz - cz;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

int TerrainMesh::heightIndex(int ix, int iz) const {
    return iz * res_ + ix;
}

float TerrainMesh::heightAtGrid(int ix, int iz) const {
    ix = std::clamp(ix, 0, res_ - 1);
    iz = std::clamp(iz, 0, res_ - 1);
    return heights_[static_cast<size_t>(heightIndex(ix, iz))];
}

float TerrainMesh::wetnessAtGrid(int ix, int iz) const {
    ix = std::clamp(ix, 0, res_ - 1);
    iz = std::clamp(iz, 0, res_ - 1);
    return wetness_[static_cast<size_t>(heightIndex(ix, iz))];
}

float TerrainMesh::riverMaskAtGrid(int ix, int iz) const {
    ix = std::clamp(ix, 0, res_ - 1);
    iz = std::clamp(iz, 0, res_ - 1);
    if (riverMask_.empty())
        return 0.0f;
    return riverMask_[static_cast<size_t>(heightIndex(ix, iz))];
}

void TerrainMesh::generateHeights(uint32_t seed) {
    const float seedOff = static_cast<float>(seed) * 0.013f;
    const float half = static_cast<float>(res_) * kCellSize * 0.5f;
    for (int iz = 0; iz < res_; ++iz) {
        for (int ix = 0; ix < res_; ++ix) {
            const float wx = origin_.x + static_cast<float>(ix) * kCellSize;
            const float wz = origin_.y + static_cast<float>(iz) * kCellSize;
            const float nx = (wx + seedOff) * 0.018f;
            const float nz = (wz + seedOff) * 0.018f;
            float h = fbm(nx, nz) * 22.0f + fbm(nx * 2.3f, nz * 2.1f) * 8.0f;
            h += fbm(nx * 4.1f + 2.0f, nz * 3.7f - 1.0f) * 3.5f;
            const float dist = std::sqrt(wx * wx + wz * wz) / (half * 1.1f);
            h *= glm::smoothstep(1.15f, 0.35f, 1.0f - dist);
            h += 4.0f;
            heights_[static_cast<size_t>(heightIndex(ix, iz))] = h;
        }
    }
}

void TerrainMesh::carveRivers(uint32_t seed) {
    wetness_.assign(static_cast<size_t>(res_ * res_), 0.0f);
    riverMask_.assign(static_cast<size_t>(res_ * res_), 0.0f);
    const float half = static_cast<float>(res_) * kCellSize * 0.5f;

    std::vector<float> sorted = heights_;
    const size_t idx = sorted.size() > 0 ? static_cast<size_t>(sorted.size() * 0.28f) : 0;
    std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(idx), sorted.end());
    waterLevel_ = sorted[idx] + 1.05f;

    struct RiverSeg {
        glm::vec2 a;
        glm::vec2 b;
        float halfWidth;
    };
    const float s = static_cast<float>(seed) * 0.01f;
    const RiverSeg rivers[] = {
        {glm::vec2(-half * 0.92f, -18.0f + std::sin(s) * 12.0f), glm::vec2(half * 0.92f, 22.0f + std::cos(s * 1.3f) * 10.0f),
         7.5f},
        {glm::vec2(-35.0f + std::cos(s * 0.7f) * 8.0f, -half * 0.85f),
         glm::vec2(40.0f + std::sin(s * 1.1f) * 6.0f, half * 0.88f), 6.0f},
        {glm::vec2(-half * 0.55f, half * 0.35f), glm::vec2(half * 0.45f, -half * 0.25f), 4.5f},
    };

    for (int iz = 0; iz < res_; ++iz) {
        for (int ix = 0; ix < res_; ++ix) {
            const float wx = origin_.x + static_cast<float>(ix) * kCellSize;
            const float wz = origin_.y + static_cast<float>(iz) * kCellSize;
            float minD = 1e9f;
            for (const RiverSeg& r : rivers) {
                minD = std::min(minD, distToSegment2D(wx, wz, r.a.x, r.a.y, r.b.x, r.b.y));
            }
            float w = 0.0f;
            for (const RiverSeg& r : rivers) {
                const float d = distToSegment2D(wx, wz, r.a.x, r.a.y, r.b.x, r.b.y);
                w = std::max(w, glm::smoothstep(r.halfWidth, r.halfWidth * 0.25f, d));
            }
            const float bank = glm::smoothstep(10.0f, 2.5f, minD) * 0.28f;
            riverMask_[static_cast<size_t>(heightIndex(ix, iz))] = w;
            wetness_[static_cast<size_t>(heightIndex(ix, iz))] = glm::clamp(w * 0.92f + bank, 0.0f, 1.0f);

            if (w > 0.02f) {
                float& h = heights_[static_cast<size_t>(heightIndex(ix, iz))];
                const float bed = waterLevel_ - 1.35f - w * 0.4f;
                h = glm::min(h, glm::mix(h, bed, glm::smoothstep(0.0f, 1.0f, w)));
            }
        }
    }
}

void TerrainMesh::buildTerrainMesh() {
    constexpr int kSub = 2;
    const int vRes = (res_ - 1) * kSub + 1;

    vertices_.clear();
    std::vector<unsigned int> indices;
    indices.reserve(static_cast<size_t>((vRes - 1) * (vRes - 1) * 6));

    const auto gridIndex = [&](int ix, int iz) -> int { return iz * vRes + ix; };

    const auto emitVert = [&](int ix, int iz) {
        const float gx = static_cast<float>(ix) / static_cast<float>(kSub);
        const float gz = static_cast<float>(iz) / static_cast<float>(kSub);
        const float wx = origin_.x + gx * kCellSize;
        const float wz = origin_.y + gz * kCellSize;
        const float wy = sampleHeight(wx, wz);
        const glm::vec3 n = sampleNormal(wx, wz);
        const float wet = sampleWetness(wx, wz);
        vertices_.push_back(wx);
        vertices_.push_back(wy);
        vertices_.push_back(wz);
        vertices_.push_back(n.x);
        vertices_.push_back(n.y);
        vertices_.push_back(n.z);
        vertices_.push_back(wx);
        vertices_.push_back(wz);
        vertices_.push_back(wet);
    };

    for (int iz = 0; iz < vRes; ++iz) {
        for (int ix = 0; ix < vRes; ++ix)
            emitVert(ix, iz);
    }

    for (int iz = 0; iz < vRes - 1; ++iz) {
        for (int ix = 0; ix < vRes - 1; ++ix) {
            const unsigned int i00 = static_cast<unsigned int>(gridIndex(ix, iz));
            const unsigned int i10 = static_cast<unsigned int>(gridIndex(ix + 1, iz));
            const unsigned int i01 = static_cast<unsigned int>(gridIndex(ix, iz + 1));
            const unsigned int i11 = static_cast<unsigned int>(gridIndex(ix + 1, iz + 1));
            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i01);
            indices.push_back(i10);
            indices.push_back(i11);
        }
    }

    indexCount_ = static_cast<int>(indices.size());

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(float)), vertices_.data(),
                 GL_STATIC_DRAW);
    constexpr GLsizei stride = static_cast<GLsizei>(9 * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(8 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void TerrainMesh::buildWaterMesh() {
    constexpr int kSub = 2;
    const int vRes = (res_ - 1) * kSub + 1;

    waterVertices_.clear();
    std::vector<unsigned int> indices;

    const auto gridIndex = [&](int ix, int iz) -> int { return iz * vRes + ix; };

    const auto emitWaterVert = [&](int ix, int iz) {
        const float gx = static_cast<float>(ix) / static_cast<float>(kSub);
        const float gz = static_cast<float>(iz) / static_cast<float>(kSub);
        const float wx = origin_.x + gx * kCellSize;
        const float wz = origin_.y + gz * kCellSize;
        const float ground = sampleHeight(wx, wz);
        const float river = sampleRiverMask(wx, wz);
        const float wet = river;
        const float surfaceY = waterLevel_;
        (void)ground;
        waterVertices_.push_back(wx);
        waterVertices_.push_back(surfaceY);
        waterVertices_.push_back(wz);
        waterVertices_.push_back(wet);
    };

    for (int iz = 0; iz < vRes; ++iz) {
        for (int ix = 0; ix < vRes; ++ix)
            emitWaterVert(ix, iz);
    }

    for (int iz = 0; iz < vRes - 1; ++iz) {
        for (int ix = 0; ix < vRes - 1; ++ix) {
            const float gx = static_cast<float>(ix) / static_cast<float>(kSub);
            const float gz = static_cast<float>(iz) / static_cast<float>(kSub);
            const float wx0 = origin_.x + gx * kCellSize;
            const float wz0 = origin_.y + gz * kCellSize;
            const float wx1 = wx0 + kCellSize / static_cast<float>(kSub);
            const float wz1 = wz0 + kCellSize / static_cast<float>(kSub);
            const float h00 = sampleHeight(wx0, wz0);
            const float h10 = sampleHeight(wx1, wz0);
            const float h01 = sampleHeight(wx0, wz1);
            const float h11 = sampleHeight(wx1, wz1);
            const float r00 = sampleRiverMask(wx0, wz0);
            const float r10 = sampleRiverMask(wx1, wz0);
            const float r01 = sampleRiverMask(wx0, wz1);
            const float r11 = sampleRiverMask(wx1, wz1);
            if (r00 < 0.42f && r10 < 0.42f && r01 < 0.42f && r11 < 0.42f)
                continue;
            if (h00 > waterLevel_ + 0.25f && h10 > waterLevel_ + 0.25f && h01 > waterLevel_ + 0.25f &&
                h11 > waterLevel_ + 0.25f)
                continue;
            const unsigned int i00 = static_cast<unsigned int>(gridIndex(ix, iz));
            const unsigned int i10 = static_cast<unsigned int>(gridIndex(ix + 1, iz));
            const unsigned int i01 = static_cast<unsigned int>(gridIndex(ix, iz + 1));
            const unsigned int i11 = static_cast<unsigned int>(gridIndex(ix + 1, iz + 1));
            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i01);
            indices.push_back(i10);
            indices.push_back(i11);
        }
    }

    waterIndexCount_ = static_cast<int>(indices.size());
    if (waterIndexCount_ <= 0)
        return;

    glBindVertexArray(waterVao_);
    glBindBuffer(GL_ARRAY_BUFFER, waterVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(waterVertices_.size() * sizeof(float)),
                 waterVertices_.data(), GL_STATIC_DRAW);
    const GLsizei wStride = static_cast<GLsizei>(4 * sizeof(float));
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, wStride, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, wStride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);
}

bool TerrainMesh::build(int resolution, uint32_t seed) {
    shutdownGpu();
    res_ = std::max(8, resolution);
    const float half = static_cast<float>(res_) * kCellSize * 0.5f;
    origin_ = glm::vec2(-half, -half);
    heights_.assign(static_cast<size_t>(res_ * res_), 0.0f);

    generateHeights(seed);
    carveRivers(seed);

    if (heights_.empty())
        return false;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glGenVertexArrays(1, &waterVao_);
    glGenBuffers(1, &waterVbo_);
    glGenBuffers(1, &waterEbo_);

    buildTerrainMesh();
    buildWaterMesh();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return vao_ != 0;
}

void TerrainMesh::uploadGpu() {}

void TerrainMesh::shutdownGpu() {
    if (waterEbo_) {
        glDeleteBuffers(1, &waterEbo_);
        waterEbo_ = 0;
    }
    if (waterVbo_) {
        glDeleteBuffers(1, &waterVbo_);
        waterVbo_ = 0;
    }
    if (waterVao_) {
        glDeleteVertexArrays(1, &waterVao_);
        waterVao_ = 0;
    }
    if (ebo_) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    indexCount_ = 0;
    waterIndexCount_ = 0;
}

void TerrainMesh::draw() const {
    if (!vao_ || indexCount_ <= 0)
        return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void TerrainMesh::drawWater() const {
    if (!waterVao_ || waterIndexCount_ <= 0)
        return;
    glBindVertexArray(waterVao_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawElements(GL_TRIANGLES, waterIndexCount_, GL_UNSIGNED_INT, nullptr);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

float TerrainMesh::sampleHeight(float worldX, float worldZ) const {
    if (heights_.empty())
        return 0.0f;
    const float lx = (worldX - origin_.x) / kCellSize;
    const float lz = (worldZ - origin_.y) / kCellSize;
    const int x0 = static_cast<int>(std::floor(lx));
    const int z0 = static_cast<int>(std::floor(lz));
    const float tx = lx - static_cast<float>(x0);
    const float tz = lz - static_cast<float>(z0);
    const float h00 = heightAtGrid(x0, z0);
    const float h10 = heightAtGrid(x0 + 1, z0);
    const float h01 = heightAtGrid(x0, z0 + 1);
    const float h11 = heightAtGrid(x0 + 1, z0 + 1);
    const float hx0 = glm::mix(h00, h10, tx);
    const float hx1 = glm::mix(h01, h11, tx);
    return glm::mix(hx0, hx1, tz);
}

float TerrainMesh::sampleWetness(float worldX, float worldZ) const {
    if (wetness_.empty())
        return 0.0f;
    const float lx = (worldX - origin_.x) / kCellSize;
    const float lz = (worldZ - origin_.y) / kCellSize;
    const int x0 = static_cast<int>(std::floor(lx));
    const int z0 = static_cast<int>(std::floor(lz));
    const float tx = lx - static_cast<float>(x0);
    const float tz = lz - static_cast<float>(z0);
    const float w00 = wetnessAtGrid(x0, z0);
    const float w10 = wetnessAtGrid(x0 + 1, z0);
    const float w01 = wetnessAtGrid(x0, z0 + 1);
    const float w11 = wetnessAtGrid(x0 + 1, z0 + 1);
    return glm::mix(glm::mix(w00, w10, tx), glm::mix(w01, w11, tx), tz);
}

float TerrainMesh::sampleRiverMask(float worldX, float worldZ) const {
    if (riverMask_.empty())
        return 0.0f;
    const float lx = (worldX - origin_.x) / kCellSize;
    const float lz = (worldZ - origin_.y) / kCellSize;
    const int x0 = static_cast<int>(std::floor(lx));
    const int z0 = static_cast<int>(std::floor(lz));
    const float tx = lx - static_cast<float>(x0);
    const float tz = lz - static_cast<float>(z0);
    const float r00 = riverMaskAtGrid(x0, z0);
    const float r10 = riverMaskAtGrid(x0 + 1, z0);
    const float r01 = riverMaskAtGrid(x0, z0 + 1);
    const float r11 = riverMaskAtGrid(x0 + 1, z0 + 1);
    return glm::mix(glm::mix(r00, r10, tx), glm::mix(r01, r11, tx), tz);
}

bool TerrainMesh::sampleIsWater(float worldX, float worldZ) const {
    return sampleRiverMask(worldX, worldZ) > 0.45f && sampleHeight(worldX, worldZ) < waterLevel_ + 0.2f;
}

glm::vec3 TerrainMesh::sampleNormal(float worldX, float worldZ) const {
    const float e = kCellSize * 0.5f;
    const float y0 = sampleHeight(worldX, worldZ);
    const float yx = sampleHeight(worldX + e, worldZ);
    const float yz = sampleHeight(worldX, worldZ + e);
    return glm::normalize(glm::vec3(y0 - yx, e, y0 - yz));
}

bool TerrainMesh::raycast(const glm::vec3& origin, const glm::vec3& dir, float& maxDist, glm::vec3& hitWorld,
                          glm::vec3& hitNormal) const {
    if (sampleIsWater(origin.x, origin.z))
        return false;
    glm::vec3 d = dir;
    const float len = glm::length(d);
    if (len < 1e-6f)
        return false;
    d /= len;

    float t = 0.0f;
    glm::vec3 lastP = origin;
    float lastDiff = sampleHeight(lastP.x, lastP.z) - lastP.y;
    for (int i = 0; i < 120; ++i) {
        t += maxDist / 120.0f;
        if (t > maxDist)
            return false;
        const glm::vec3 p = origin + d * t;
        if (sampleIsWater(p.x, p.z)) {
            lastP = p;
            lastDiff = -1.0f;
            continue;
        }
        const float diff = sampleHeight(p.x, p.z) - p.y;
        if (diff >= 0.0f && lastDiff < 0.0f) {
            hitWorld = glm::mix(lastP, p, 0.5f);
            hitNormal = sampleNormal(hitWorld.x, hitWorld.z);
            maxDist = glm::length(hitWorld - origin);
            return true;
        }
        lastP = p;
        lastDiff = diff;
    }
    return false;
}

} // namespace meshcraft
