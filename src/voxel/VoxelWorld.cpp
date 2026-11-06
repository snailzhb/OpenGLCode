#include "voxel/VoxelWorld.h"

#include "game/SceneComposer.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <fstream>
#include <functional>

namespace voxel {

namespace {

int floorDiv(int a, int b) {
    const int q = a / b;
    const int r = a % b;
    if ((r != 0) && ((r > 0) != (b > 0)))
        return q - 1;
    return q;
}

inline float fractf(float t) {
    return t - std::floor(t);
}

inline float hash12(float x, float y) {
    return fractf(std::sin(x * 127.1f + y * 311.7f) * 43758.5453f);
}

/** 村落铺地与建房对齐用：忽略路灯柱、木桥面、烟囱等「脚手架」顶面，否则会抬高 maxSy 造成地坪悬空 */
inline bool isVillageScaffoldFoot(BlockId b) {
    return b == BlockId::Wood || b == BlockId::MossBrick || b == BlockId::HayThatch || b == BlockId::RoofTerracotta ||
           b == BlockId::Plank || b == BlockId::Slate || b == BlockId::Shingle || b == BlockId::DarkWood ||
           b == BlockId::Marble || isLamp(b);
}

/** 平滑值噪声 [0,1] */
float valueNoise2(float x, float z) {
    const int xi = static_cast<int>(std::floor(x));
    const int zi = static_cast<int>(std::floor(z));
    const float fx = x - static_cast<float>(xi);
    const float fz = z - static_cast<float>(zi);
    const float u = fx * fx * (3.0f - 2.0f * fx);
    const float v = fz * fz * (3.0f - 2.0f * fz);
    const float a = hash12(static_cast<float>(xi), static_cast<float>(zi));
    const float b = hash12(static_cast<float>(xi + 1), static_cast<float>(zi));
    const float c = hash12(static_cast<float>(xi), static_cast<float>(zi + 1));
    const float d = hash12(static_cast<float>(xi + 1), static_cast<float>(zi + 1));
    const float ab = a + (b - a) * u;
    const float cd = c + (d - c) * u;
    return ab + (cd - ab) * v;
}

float fbm2(float x, float z, int octaves) {
    float sum = 0.0f;
    float amp = 0.5f;
    float xs = x;
    float zs = z;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * valueNoise2(xs, zs);
        xs *= 2.02f;
        zs *= 2.03f;
        amp *= 0.5f;
    }
    return sum;
}

/** 曲流河：|turn| 接近 0 为河床中心；返回 [0,1]，越大越靠近河床/河滩 */
float riverBankness(float xf, float zf) {
    const float px = xf * 0.017f + 0.35f * std::sin(zf * 0.0018f) * 14.0f;
    const float pz = zf * 0.015f + 0.28f * std::cos(xf * 0.0016f) * 12.0f;
    const float turn =
        std::sin(px * 0.42f) * std::cos(pz * 0.38f) + 0.22f * std::sin((xf + zf) * 0.0028f);
    const float rd = std::abs(turn);
    const float inner = 0.13f;
    const float outer = 0.26f;
    if (rd >= outer)
        return 0.0f;
    if (rd <= inner)
        return 1.0f;
    const float t = (outer - rd) / (outer - inner);
    return t * t;
}

/** 连续地表高度（米块坐标）：平原为主 + 低丘 + 河谷下切 */
float terrainHeightF(float xf, float zf) {
    /* 大尺度「陆块」起伏：整体像地球表面缓变 */
    const float continent = 2.0f * std::sin(xf * 0.0036f + 1.05f) + 1.7f * std::cos(zf * 0.0031f - 0.33f) +
                            0.9f * std::sin((xf + zf * 0.72f) * 0.0022f);
    /* 平原上的长波起伏（层数少，加快 chunk 生成） */
    const float gentle = 2.2f * (fbm2(xf * 0.019f, zf * 0.018f, 2) - 0.5f);
    /* 小丘陵 */
    const float hillMask = 0.48f + 0.52f * (0.5f + 0.5f * std::sin(xf * 0.010f + zf * 0.012f));
    const float hills = 1.55f * (fbm2(xf * 0.062f, zf * 0.065f, 2) - 0.5f) * hillMask;

    float h = 37.5f + continent + gentle + hills;

    /* 河谷下切深度随河床噪声略变 */
    const float bank = riverBankness(xf, zf);
    if (bank > 0.001f) {
        const float depth = (4.1f + 1.15f * valueNoise2(xf * 0.09f, zf * 0.09f)) * bank * bank;
        h -= depth;
    }
    return h;
}

/** 由列缓存的 hi / bank 生成方块（含河流水面） */
BlockId terrainBlockFromColumn(int wx, int wy, int wz, int hi, float bank) {
    (void)wz;
    const int sea = static_cast<int>(std::floor(kSeaLevel));
    /* 河流水面略高于海平面整数格，河谷里 hi 以上充水 */
    const int riverTop = sea + 3;

    if (wy < hi - 3)
        return BlockId::Stone;
    if (wy < hi - 1)
        return BlockId::Dirt;
    if (wy < hi) {
        if (hi <= sea + 1)
            return BlockId::Sand;
        if (bank > 0.12f)
            return BlockId::Sand;
        return BlockId::Grass;
    }
    if (bank > 0.045f && wy >= hi && wy < riverTop)
        return BlockId::Water;
    if (wy < sea)
        return BlockId::Water;
    return BlockId::Air;
}

BlockId terrainBlockAt(int wx, int wy, int wz) {
    const float xf = static_cast<float>(wx);
    const float zf = static_cast<float>(wz);
    const int hi = static_cast<int>(std::floor(terrainHeightF(xf, zf) + 1.0e-4f));
    const float bank = riverBankness(xf, zf);
    return terrainBlockFromColumn(wx, wy, wz, hi, bank);
}

/** 列内最高非空气非水固体，用于地表装饰 */
int topSolidLy(const Chunk& ch, int lx, int lz) {
    for (int ly = kChunkSize - 1; ly >= 0; --ly) {
        const BlockId b = ch.getLocal(lx, ly, lz);
        if (b != BlockId::Air && b != BlockId::Water)
            return ly;
    }
    return -1;
}

} // namespace

int VoxelWorld::terrainHeight(int wx, int wz) {
    const float h = terrainHeightF(static_cast<float>(wx), static_cast<float>(wz));
    return static_cast<int>(std::floor(h + 1.0e-4f));
}

bool VoxelWorld::hasAnyGlassMesh() const {
    for (const auto& kv : chunks_) {
        if (kv.second->hasGlass())
            return true;
    }
    return false;
}

uint64_t VoxelWorld::packChunk(int cx, int cy, int cz) {
    const auto enc = [](int v) -> uint32_t { return static_cast<uint32_t>(v + 0x8000); };
    return (static_cast<uint64_t>(enc(cx)) << 42) | (static_cast<uint64_t>(enc(cy)) << 21) | static_cast<uint64_t>(enc(cz));
}

static void unpackChunkKey(uint64_t k, int& cx, int& cy, int& cz) {
    const uint32_t encCx = static_cast<uint32_t>((k >> 42) & 0x1FFFFFu);
    const uint32_t encCy = static_cast<uint32_t>((k >> 21) & 0x1FFFFFu);
    const uint32_t encCz = static_cast<uint32_t>(k & 0x1FFFFFu);
    cx = static_cast<int>(encCx) - 0x8000;
    cy = static_cast<int>(encCy) - 0x8000;
    cz = static_cast<int>(encCz) - 0x8000;
}

static float chunkCenterDist2(const Chunk* ch, const glm::vec3& p) {
    const float mx = static_cast<float>(ch->cx * kChunkSize + kChunkSize / 2);
    const float my = static_cast<float>(ch->cy * kChunkSize + kChunkSize / 2);
    const float mz = static_cast<float>(ch->cz * kChunkSize + kChunkSize / 2);
    const glm::vec3 d(mx - p.x, my - p.y, mz - p.z);
    return glm::dot(d, d);
}

void VoxelWorld::worldToChunk(int wx, int wy, int wz, int& cx, int& cy, int& cz, int& lx, int& ly, int& lz) {
    cx = floorDiv(wx, kChunkSize);
    cy = floorDiv(wy, kChunkSize);
    cz = floorDiv(wz, kChunkSize);
    lx = wx - cx * kChunkSize;
    ly = wy - cy * kChunkSize;
    lz = wz - cz * kChunkSize;
}

VoxelWorld::VoxelWorld() {
    smokePuffs_.reserve(4096);
    scaledVoxels_.reserve(4096);
    sceneryFlags_.reserve(192);
}

Chunk* VoxelWorld::getChunkMutable(int cx, int cy, int cz) {
    if (cy < kChunkYMin || cy > kChunkYMax)
        return nullptr;
    const uint64_t k = packChunk(cx, cy, cz);
    auto it = chunks_.find(k);
    if (it == chunks_.end())
        return nullptr;
    return it->second.get();
}

const Chunk* VoxelWorld::getChunkConst(int cx, int cy, int cz) const {
    if (cy < kChunkYMin || cy > kChunkYMax)
        return nullptr;
    const uint64_t k = packChunk(cx, cy, cz);
    auto it = chunks_.find(k);
    if (it == chunks_.end())
        return nullptr;
    return it->second.get();
}

BlockId VoxelWorld::getBlock(int wx, int wy, int wz) const {
    int cx, cy, cz, lx, ly, lz;
    worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    const Chunk* ch = getChunkConst(cx, cy, cz);
    if (!ch)
        return terrainBlockAt(wx, wy, wz);
    return ch->getLocal(lx, ly, lz);
}

void VoxelWorld::setBlock(int wx, int wy, int wz, BlockId b) {
    int cx, cy, cz, lx, ly, lz;
    worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    if (cy < kChunkYMin || cy > kChunkYMax)
        return;
    ensureChunk(cx, cy, cz);
    Chunk* ch = getChunkMutable(cx, cy, cz);
    if (!ch)
        return;
    ch->setLocal(lx, ly, lz, b);
    markNeighborsDirty(wx, wy, wz);
}

void VoxelWorld::markNeighborsDirty(int wx, int wy, int wz) {
    int cx, cy, cz, lx, ly, lz;
    worldToChunk(wx, wy, wz, cx, cy, cz, lx, ly, lz);
    auto touch = [&](int dcx, int dcy, int dcz) {
        Chunk* c = getChunkMutable(cx + dcx, cy + dcy, cz + dcz);
        if (c)
            c->markDirty();
    };
    touch(0, 0, 0);
    if (lx == 0)
        touch(-1, 0, 0);
    if (lx == kChunkSize - 1)
        touch(1, 0, 0);
    if (ly == 0)
        touch(0, -1, 0);
    if (ly == kChunkSize - 1)
        touch(0, 1, 0);
    if (lz == 0)
        touch(0, 0, -1);
    if (lz == kChunkSize - 1)
        touch(0, 0, 1);
}

void VoxelWorld::generateChunk(Chunk& ch) {
    const int ox = ch.cx * kChunkSize;
    const int oy = ch.cy * kChunkSize;
    const int oz = ch.cz * kChunkSize;
    /* 按 (x,z) 列只算一次高度与河谷掩码，避免每体素重复 fbm（卡顿主因之一） */
    for (int lz = 0; lz < kChunkSize; ++lz) {
        for (int lx = 0; lx < kChunkSize; ++lx) {
            const int wx = ox + lx;
            const int wz = oz + lz;
            const float xf = static_cast<float>(wx);
            const float zf = static_cast<float>(wz);
            const int hi = static_cast<int>(std::floor(terrainHeightF(xf, zf) + 1.0e-4f));
            const float bank = riverBankness(xf, zf);
            for (int ly = 0; ly < kChunkSize; ++ly) {
                const int wy = oy + ly;
                ch.setLocalBulk(lx, ly, lz, terrainBlockFromColumn(wx, wy, wz, hi, bank));
            }
        }
    }
    ch.markMeshDirty();
}

void VoxelWorld::streamAround(int wx, int wz, int halfExtentChunks) {
    const int cx = floorDiv(wx, kChunkSize);
    const int cz = floorDiv(wz, kChunkSize);
    for (int dcx = -halfExtentChunks; dcx <= halfExtentChunks; ++dcx) {
        for (int dcz = -halfExtentChunks; dcz <= halfExtentChunks; ++dcz) {
            for (int cy = kChunkYMin; cy <= kChunkYMax; ++cy) {
                const uint64_t k = packChunk(cx + dcx, cy, cz + dcz);
                if (chunks_.count(k) != 0)
                    continue;
                pendingCreate_.insert(k);
            }
        }
    }
}

void VoxelWorld::pruneChunksAwayFromCamera(const glm::vec3& camWorld, int streamHalfExtentChunks) {
    const int keepR = streamHalfExtentChunks + kChunkPruneMarginChunks;
    const int cxc = floorDiv(static_cast<int>(std::floor(camWorld.x)), kChunkSize);
    const int czc = floorDiv(static_cast<int>(std::floor(camWorld.z)), kChunkSize);

    auto tooFar = [&](int cx, int cz) {
        const int dx = std::abs(cx - cxc);
        const int dz = std::abs(cz - czc);
        return dx > keepR || dz > keepR;
    };

    std::vector<uint64_t> eraseKeys;
    eraseKeys.reserve(256);
    for (const auto& kv : chunks_) {
        int cx = 0, cy = 0, cz = 0;
        unpackChunkKey(kv.first, cx, cy, cz);
        if (tooFar(cx, cz))
            eraseKeys.push_back(kv.first);
    }
    for (uint64_t k : eraseKeys)
        chunks_.erase(k);

    /* pending 队列里过远的创建请求一并丢掉，避免 set 无限膨胀 */
    if (!pendingCreate_.empty()) {
        std::vector<uint64_t> pendErase;
        pendErase.reserve(pendingCreate_.size());
        for (uint64_t k : pendingCreate_) {
            int cx = 0, cy = 0, cz = 0;
            unpackChunkKey(k, cx, cy, cz);
            if (tooFar(cx, cz))
                pendErase.push_back(k);
        }
        for (uint64_t k : pendErase)
            pendingCreate_.erase(k);
    }
}

void VoxelWorld::ensureChunk(int cx, int cy, int cz) {
    if (cy < kChunkYMin || cy > kChunkYMax)
        return;
    const uint64_t k = packChunk(cx, cy, cz);
    if (chunks_.count(k))
        return;
    pendingCreate_.erase(k);
    auto ch = std::make_unique<Chunk>(cx, cy, cz);
    generateChunk(*ch);
    decorateChunk(*ch);
    chunks_[k] = std::move(ch);
    tryPlaceRegionalVillagesForChunk(cx, cy, cz);
}

void VoxelWorld::pumpPendingChunkCreations(int maxCreates, const glm::vec3& camWorld) {
    if (maxCreates <= 0 || pendingCreate_.empty())
        return;
    std::vector<std::pair<float, uint64_t>> scored;
    scored.reserve(pendingCreate_.size());
    for (uint64_t key : pendingCreate_) {
        int tcx = 0, tcy = 0, tcz = 0;
        unpackChunkKey(key, tcx, tcy, tcz);
        const float mx = static_cast<float>(tcx * kChunkSize + kChunkSize / 2);
        const float my = static_cast<float>(tcy * kChunkSize + kChunkSize / 2);
        const float mz = static_cast<float>(tcz * kChunkSize + kChunkSize / 2);
        const float dx = mx - camWorld.x;
        const float dy = my - camWorld.y;
        const float dz = mz - camWorld.z;
        scored.push_back(std::make_pair(dx * dx + dy * dy + dz * dz, key));
    }
    const int take = std::min(maxCreates, static_cast<int>(scored.size()));
    std::partial_sort(scored.begin(), scored.begin() + take, scored.end(),
                      [](const std::pair<float, uint64_t>& a, const std::pair<float, uint64_t>& b) {
                          return a.first < b.first;
                      });
    for (int i = 0; i < take; ++i) {
        const uint64_t k = scored[static_cast<size_t>(i)].second;
        if (pendingCreate_.erase(k) == 0)
            continue;
        if (chunks_.count(k) != 0)
            continue;
        int ccx = 0, ccy = 0, ccz = 0;
        unpackChunkKey(k, ccx, ccy, ccz);
        auto ch = std::make_unique<Chunk>(ccx, ccy, ccz);
        generateChunk(*ch);
        decorateChunk(*ch);
        chunks_[k] = std::move(ch);
        tryPlaceRegionalVillagesForChunk(ccx, ccy, ccz);
    }
}

void VoxelWorld::rebuildDirtyMeshes(int maxChunkRebuilds, const glm::vec3* sortCam, float maxCpuSec) {
    std::function<BlockId(int, int, int)> getter = [this](int x, int y, int z) { return getBlock(x, y, z); };
    std::vector<Chunk*> dirty;
    dirty.reserve(128);
    for (auto& kv : chunks_) {
        Chunk* ch = kv.second.get();
        if (ch->meshRebuildPending())
            dirty.push_back(ch);
    }
    if (dirty.empty())
        return;
    if (sortCam) {
        const glm::vec3 p = *sortCam;
        std::sort(dirty.begin(), dirty.end(), [&](Chunk* a, Chunk* b) {
            return chunkCenterDist2(a, p) < chunkCenterDist2(b, p);
        });
    }
    using Clock = std::chrono::steady_clock;
    const Clock::time_point t0 = Clock::now();
    int n = 0;
    for (Chunk* ch : dirty) {
        if (n >= maxChunkRebuilds)
            break;
        if (maxCpuSec > 0.0f) {
            const float elapsed =
                std::chrono::duration<float>(Clock::now() - t0).count();
            if (elapsed >= maxCpuSec)
                break;
        }
        ch->rebuildMesh(getter);
        ++n;
    }
}

void VoxelWorld::uploadMeshes(int maxChunkUploads, const glm::vec3* sortCam, float maxCpuSec) {
    std::vector<Chunk*> pending;
    pending.reserve(128);
    for (auto& kv : chunks_) {
        Chunk* ch = kv.second.get();
        if (ch->gpuUploadPending())
            pending.push_back(ch);
    }
    if (pending.empty())
        return;
    if (sortCam) {
        const glm::vec3 p = *sortCam;
        std::sort(pending.begin(), pending.end(), [&](Chunk* a, Chunk* b) {
            return chunkCenterDist2(a, p) < chunkCenterDist2(b, p);
        });
    }
    using Clock = std::chrono::steady_clock;
    const Clock::time_point t0 = Clock::now();
    int n = 0;
    for (Chunk* ch : pending) {
        if (n >= maxChunkUploads)
            break;
        if (maxCpuSec > 0.0f) {
            const float elapsed =
                std::chrono::duration<float>(Clock::now() - t0).count();
            if (elapsed >= maxCpuSec)
                break;
        }
        ch->uploadIfDirty();
        ++n;
    }
}

void VoxelWorld::flushAllPendingMeshes() {
    std::function<BlockId(int, int, int)> getter = [this](int x, int y, int z) { return getBlock(x, y, z); };
    for (auto& kv : chunks_) {
        Chunk* ch = kv.second.get();
        if (ch->meshRebuildPending())
            ch->rebuildMesh(getter);
    }
    for (auto& kv : chunks_) {
        Chunk* ch = kv.second.get();
        if (ch->gpuUploadPending())
            ch->uploadIfDirty();
    }
}

void VoxelWorld::gatherNearbyLamps(const glm::vec3& cam, int maxLamps, float searchRadius, std::vector<glm::vec4>& outPacked,
                                   int& outCount) const {
    outPacked.clear();
    outCount = 0;
    const float r2max = searchRadius * searchRadius;

    struct Cand {
        float d2;
        glm::vec4 posR;
        glm::vec4 rgbI;
    };
    std::vector<Cand> cands;
    cands.reserve(128);

    auto tintRgb = [](BlockId id, int wx, int wy, int wz) -> glm::vec3 {
        glm::vec3 base(1.0f, 0.78f, 0.48f);
        if (id == BlockId::LampWhite)
            base = glm::vec3(1.0f, 1.0f, 1.0f);
        else if (id == BlockId::LampCyan)
            base = glm::vec3(0.42f, 0.92f, 1.0f);
        else if (id == BlockId::LampMagenta)
            base = glm::vec3(1.0f, 0.42f, 0.95f);
        const float px = static_cast<float>(wx) * 0.37f + static_cast<float>(wz) * 0.09f;
        const float py = static_cast<float>(wy) * 0.11f + static_cast<float>(wz) * 0.21f;
        float t = std::fmod(std::sin(px * 127.1f + py * 311.7f) * 43758.547f, 1.0f);
        if (t < 0.0f)
            t += 1.0f;
        const glm::vec3 a(1.0f, 0.72f, 0.38f), b(0.42f, 0.95f, 1.05f), c(1.05f, 0.45f, 0.92f), d(0.55f, 1.0f, 0.52f);
        glm::vec3 tint = glm::mix(glm::mix(a, b, t), glm::mix(c, d, std::fmod(t * 7.13f, 1.0f)), std::fmod(t * 13.7f, 1.0f));
        return glm::clamp(base * tint * 1.05f, glm::vec3(0.2f), glm::vec3(2.1f));
    };

    for (const auto& kv : chunks_) {
        const Chunk* ch = kv.second.get();
        const int ox = ch->cx * kChunkSize;
        const int oy = ch->cy * kChunkSize;
        const int oz = ch->cz * kChunkSize;
        const glm::vec3 mid(static_cast<float>(ox + 8), static_cast<float>(oy + 8), static_cast<float>(oz + 8));
        const glm::vec3 dm = mid - cam;
        if (glm::dot(dm, dm) > (searchRadius + 24.0f) * (searchRadius + 24.0f))
            continue;

        for (int lz = 0; lz < kChunkSize; ++lz) {
            for (int lx = 0; lx < kChunkSize; ++lx) {
                for (int ly = 0; ly < kChunkSize; ++ly) {
                    const BlockId b = ch->getLocal(lx, ly, lz);
                    if (!isLamp(b))
                        continue;
                    const int wx = ox + lx;
                    const int wy = oy + ly;
                    const int wz = oz + lz;
                    const glm::vec3 p(static_cast<float>(wx) + 0.5f, static_cast<float>(wy) + 0.55f,
                                      static_cast<float>(wz) + 0.5f);
                    const glm::vec3 dv = p - cam;
                    const float d2 = glm::dot(dv, dv);
                    if (d2 > r2max)
                        continue;
                    const float R = 16.0f;
                    Cand cd;
                    cd.d2 = d2;
                    cd.posR = glm::vec4(p, R);
                    cd.rgbI = glm::vec4(tintRgb(b, wx, wy, wz), 1.0f);
                    cands.push_back(cd);
                }
            }
        }
    }

    const int cap = std::min(maxLamps, static_cast<int>(cands.size()));
    if (cap <= 0)
        return;
    std::partial_sort(cands.begin(), cands.begin() + cap, cands.end(),
                      [](const Cand& A, const Cand& B) { return A.d2 < B.d2; });

    outPacked.resize(static_cast<size_t>(cap) * 2);
    for (int i = 0; i < cap; ++i) {
        outPacked[static_cast<size_t>(i) * 2] = cands[static_cast<size_t>(i)].posR;
        outPacked[static_cast<size_t>(i) * 2 + 1] = cands[static_cast<size_t>(i)].rgbI;
    }
    outCount = cap;
}

void VoxelWorld::addScaledPropAt(float worldX, float worldY, float worldZ, float side, int tileId, float yawDeg) {
    SceneryScaledVoxel sv;
    sv.posScale = glm::vec4(worldX, worldY, worldZ, side);
    sv.tilePad = glm::vec4(static_cast<float>(tileId), yawDeg, 0.0f, 0.0f);
    scaledVoxels_.push_back(sv);
}

void VoxelWorld::addScaledProp(float worldX, float worldZ, float side, int tileId, float yawDeg, int hubWx,
                               int hubWz, float hubAvoidRadius) {
    if (hubAvoidRadius > 0.0f) {
        const float dx = worldX - static_cast<float>(hubWx);
        const float dz = worldZ - static_cast<float>(hubWz);
        if (dx * dx + dz * dz < hubAvoidRadius * hubAvoidRadius)
            return;
    }
    const float half = 0.5f * side;
    const int x0 = static_cast<int>(std::floor(worldX - half));
    const int x1 = static_cast<int>(std::floor(worldX + half));
    const int z0 = static_cast<int>(std::floor(worldZ - half));
    const int z1 = static_cast<int>(std::floor(worldZ + half));
    auto localGround = [this](int wx, int wz) -> int {
        for (int y = 96; y >= 2; --y) {
            const BlockId b = getBlock(wx, y, wz);
            if (!isSolid(b))
                continue;
            const BlockId ab = getBlock(wx, y + 1, wz);
            if (!(ab == BlockId::Air || isGlassFamily(ab) || ab == BlockId::Water))
                continue;
            if (isVillageScaffoldFoot(b))
                continue;
            return y;
        }
        return terrainHeight(wx, wz);
    };
    float baseY = -1.0e9f;
    for (int cx = x0; cx <= x1; ++cx) {
        for (int cz = z0; cz <= z1; ++cz)
            baseY = std::max(baseY, static_cast<float>(localGround(cx, cz)) + 1.0f);
    }
    if (baseY < -1.0e8f)
        return;
    for (int guard = 0; guard < 36; ++guard) {
        const int yCell = static_cast<int>(std::floor(baseY + 1.0e-4f));
        bool blocked = false;
        for (int cx = x0; cx <= x1 && !blocked; ++cx) {
            for (int cz = z0; cz <= z1; ++cz) {
                const BlockId b = getBlock(cx, yCell, cz);
                if (b != BlockId::Air && b != BlockId::Water) {
                    blocked = true;
                    break;
                }
            }
        }
        if (!blocked)
            break;
        baseY += 1.0f;
    }
    addScaledPropAt(worldX, baseY + 0.5f * side, worldZ, side, tileId, yawDeg);
}

void VoxelWorld::placeCompositePrefabs(int anchorWx, int anchorWz, uint32_t seed, bool hub) {
    const float cx = static_cast<float>(anchorWx);
    const float cz = static_cast<float>(anchorWz);
    const float avoid = hub ? 24.0f : 14.0f;
    const int tidStone = static_cast<int>(BlockId::Stone);
    const int tidMarble = static_cast<int>(BlockId::Marble);
    const int tidIron = static_cast<int>(BlockId::Iron);
    const int tidDark = static_cast<int>(BlockId::DarkWood);
    const int tidPlank = static_cast<int>(BlockId::Plank);
    const int tidFlower = static_cast<int>(BlockId::FlowerGrass);
    const int tidMoss = static_cast<int>(BlockId::MossStone);
    const int tidCobble = static_cast<int>(BlockId::Cobble);

    const float windX = cx + (hub ? 42.0f : 24.0f + static_cast<float>(seed % 7u));
    const float windZ = cz + (hub ? -34.0f : -20.0f + static_cast<float>((seed >> 3) % 7u));
    {
        const float half = 2.4f;
        const int x0 = static_cast<int>(std::floor(windX - half));
        const int z0 = static_cast<int>(std::floor(windZ - half));
        int sy = terrainHeight(x0, z0);
        const float base = static_cast<float>(sy + 1);
        addScaledPropAt(windX, base + 2.4f, windZ, 4.8f, tidDark, 0.0f);
        addScaledPropAt(windX, base + 5.8f, windZ, 3.6f, tidPlank, 0.0f);
        addScaledPropAt(windX, base + 8.2f, windZ, 2.8f, static_cast<int>(BlockId::Shingle), 0.0f);
        for (int b = 0; b < 4; ++b) {
            const float ang = static_cast<float>(b) * 90.0f + 18.0f;
            addScaledPropAt(windX, base + 8.5f, windZ, 6.2f, tidPlank, ang);
        }
        addScaledPropAt(windX, base + 9.4f, windZ, 0.85f, tidIron, static_cast<float>(seed % 360u));
    }

    const float wellX = cx + (hub ? -32.0f : -18.0f);
    const float wellZ = cz + (hub ? 28.0f : 14.0f);
    {
        const int wx = static_cast<int>(wellX);
        const int wz = static_cast<int>(wellZ);
        const int sy = terrainHeight(wx, wz);
        setBlock(wx, sy, wz, BlockId::Water);
        for (int i = 0; i < 8; ++i) {
            const float ang = static_cast<float>(i) * 6.2831853f / 8.0f;
            addScaledPropAt(wellX + std::cos(ang) * 1.35f, static_cast<float>(sy) + 0.55f, wellZ + std::sin(ang) * 1.35f,
                            0.95f, tidCobble, ang * 57.2957795f);
        }
        addScaledPropAt(wellX, static_cast<float>(sy) + 1.35f, wellZ, 0.75f, tidIron, 0.0f);
    }

    const int treeN = hub ? 5 : 3;
    for (int t = 0; t < treeN; ++t) {
        const float ang = static_cast<float>(t) * 6.2831853f / static_cast<float>(treeN) + static_cast<float>(seed % 100u) * 0.01f;
        const float dist = hub ? 30.0f + static_cast<float>(t % 3) * 4.0f : 20.0f + static_cast<float>(t) * 2.5f;
        const float tx = cx + std::cos(ang) * dist;
        const float tz = cz + std::sin(ang) * dist;
        if ((tx - cx) * (tx - cx) + (tz - cz) * (tz - cz) < avoid * avoid)
            continue;
        const int sy = terrainHeight(static_cast<int>(tx), static_cast<int>(tz));
        const float base = static_cast<float>(sy + 1);
        addScaledPropAt(tx, base + 1.2f, tz, 2.4f, tidDark, 0.0f);
        addScaledPropAt(tx, base + 3.0f, tz, 2.0f, tidDark, 22.0f);
        const float canopy = 3.2f + static_cast<float>(t % 2) * 0.8f;
        addScaledPropAt(tx + 0.4f, base + 4.8f, tz - 0.3f, canopy, tidFlower, 0.0f);
        addScaledPropAt(tx - 1.1f, base + 4.2f, tz + 0.6f, canopy * 0.72f, tidFlower, 35.0f);
        addScaledPropAt(tx + 0.8f, base + 5.2f, tz + 1.0f, canopy * 0.55f, static_cast<int>(BlockId::Grass), 0.0f);
    }

    const float obX = cx + (hub ? -48.0f : 26.0f);
    const float obZ = cz + (hub ? -18.0f : -22.0f);
    {
        const int sy = terrainHeight(static_cast<int>(obX), static_cast<int>(obZ));
        const float base = static_cast<float>(sy + 1);
        addScaledPropAt(obX, base + 1.0f, obZ, 2.8f, tidMarble, 0.0f);
        addScaledPropAt(obX, base + 2.6f, obZ, 2.2f, tidMoss, 0.0f);
        addScaledPropAt(obX, base + 3.8f, obZ, 1.6f, tidMarble, 15.0f);
        addScaledPropAt(obX, base + 4.9f, obZ, 1.15f, tidIron, 0.0f);
    }

    if (hub) {
        for (int i = 0; i < 12; ++i) {
            const float ang = static_cast<float>(i) * 6.2831853f / 12.0f;
            const float dist = 52.0f + static_cast<float>(i % 3) * 3.0f;
            const int tid = (i % 4 == 0) ? tidStone : (i % 4 == 1) ? tidMoss : (i % 4 == 2) ? tidMarble : tidCobble;
            const float side = (i % 4 == 0) ? 2.8f : (i % 3 == 0) ? 1.45f : 0.85f;
            addScaledProp(cx + std::cos(ang) * dist, cz + std::sin(ang) * dist, side, tid, ang * 57.2957795f, anchorWx,
                          anchorWz, avoid);
        }
    } else {
        for (int i = 0; i < 6; ++i) {
            const float ang = static_cast<float>(i) * 6.2831853f / 6.0f;
            addScaledProp(cx + std::cos(ang) * 16.0f, cz + std::sin(ang) * 16.0f, (i % 2 == 0) ? 1.55f : 0.72f,
                          (i % 2 == 0) ? tidStone : tidCobble, ang * 57.2957795f, anchorWx, anchorWz, avoid);
        }
    }
}

void VoxelWorld::tryPlaceRegionalVillagesForChunk(int cx, int cy, int cz) {
    constexpr int cell = 48;
    const int ox = cx * kChunkSize;
    const int oz = cz * kChunkSize;
    for (int ix = floorDiv(ox - cell + 1, cell); ix <= floorDiv(ox + kChunkSize - 1, cell); ++ix) {
        for (int iz = floorDiv(oz - cell + 1, cell); iz <= floorDiv(oz + kChunkSize - 1, cell); ++iz) {
            const int scx = ix * cell + cell / 2;
            const int scz = iz * cell + cell / 2;
            if (scx < ox || scx >= ox + kChunkSize || scz < oz || scz >= oz + kChunkSize)
                continue;
            const int th = terrainHeight(scx, scz);
            if (floorDiv(th, kChunkSize) != cy)
                continue;
            if (riverBankness(static_cast<float>(scx), static_cast<float>(scz)) > 0.38f)
                continue;
            const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(ix + 0x8000)) << 32) |
                                 static_cast<uint64_t>(static_cast<uint32_t>(iz + 0x8000));
            if (!regionalVillagePlaced_.insert(key).second)
                continue;
            const uint32_t seed = static_cast<uint32_t>(ix * 100003 + iz * 9176 + 17);
            const bool hub = (ix == 0 && iz == 0);
            buildRegionalVillageCluster(scx, scz, seed, hub);
        }
    }
}

void VoxelWorld::buildRegionalVillageCluster(int anchorWx, int anchorWz, uint32_t seed, bool hub) {
    const int lampHalf = hub ? 80 : 0;
    const int pad = hub ? std::max(48, lampHalf + 14) : 26;
    const int minCx = floorDiv(anchorWx - pad, kChunkSize);
    const int maxCx = floorDiv(anchorWx + pad, kChunkSize);
    const int minCz = floorDiv(anchorWz - pad, kChunkSize);
    const int maxCz = floorDiv(anchorWz + pad, kChunkSize);
    for (int ccx = minCx; ccx <= maxCx; ++ccx) {
        for (int ccz = minCz; ccz <= maxCz; ++ccz) {
            for (int ccy = kChunkYMin; ccy <= kChunkYMax; ++ccy)
                ensureChunk(ccx, ccy, ccz);
        }
    }

    auto villageGroundY = [this](int wx, int wz) -> int {
        for (int y = 96; y >= 2; --y) {
            const BlockId b = getBlock(wx, y, wz);
            if (!isSolid(b))
                continue;
            const BlockId ab = getBlock(wx, y + 1, wz);
            /* 水面上方视为「开放」，与旧 surfaceY 一致 */
            if (!(ab == BlockId::Air || isGlassFamily(ab) || ab == BlockId::Water))
                continue;
            if (isVillageScaffoldFoot(b))
                continue;
            return y;
        }
        return -1;
    };

    auto addChimneySmoke = [this](float x, float y, float z, float smokeSeed) {
        constexpr int kN = 28;
        for (int i = 0; i < kN; ++i) {
            ScenerySmokePuff p;
            const float t = static_cast<float>(i);
            const float j = 0.08f * std::sin(smokeSeed * 2.3f + t * 0.71f);
            p.posScale = glm::vec4(x + 0.11f * std::sin(t * 1.4f + smokeSeed) + j, y + t * 0.22f,
                                   z + 0.11f * std::cos(t * 1.1f + smokeSeed * 0.63f), 0.22f + t * 0.012f);
            p.tintPhase = glm::vec4(0.92f, 0.88f, 0.84f, smokeSeed + t * 0.31f);
            smokePuffs_.push_back(p);
        }
    };

    auto buildHouse = [&](int ax, int az, int fw, int fd, int wallH, float smokeSeed) {
        int maxSy = -1;
        for (int x = 0; x < fw; ++x) {
            for (int z = 0; z < fd; ++z) {
                const int s = villageGroundY(ax + x, az + z);
                if (s < 0)
                    return;
                maxSy = std::max(maxSy, s);
            }
        }
        const int f = maxSy + 1;
        for (int x = 0; x < fw; ++x) {
            for (int z = 0; z < fd; ++z) {
                const int localSy = villageGroundY(ax + x, az + z);
                if (localSy < 0)
                    continue;
                for (int yy = localSy + 1; yy < f; ++yy)
                    setBlock(ax + x, yy, az + z, BlockId::Cobble);
            }
        }
        for (int x = 0; x < fw; ++x)
            for (int z = 0; z < fd; ++z)
                setBlock(ax + x, f, az + z, BlockId::Cobble);
        for (int y = 1; y <= wallH; ++y)
            for (int x = 1; x < fw - 1; ++x)
                for (int z = 1; z < fd - 1; ++z)
                    setBlock(ax + x, f + y, az + z, BlockId::Air);
        for (int y = 1; y <= wallH; ++y)
            for (int x = 0; x < fw; ++x)
                for (int z = 0; z < fd; ++z) {
                    const bool edge = (x == 0 || x == fw - 1 || z == 0 || z == fd - 1);
                    if (!edge)
                        continue;
                    const int dx = fw / 2;
                    if (z == 0 && x == dx && y <= 2)
                        continue;
                    const bool win = (y == 2 || y == 3) && (x == 1 || x == fw - 2) && (z == 0 || z == fd - 1);
                    setBlock(ax + x, f + y, az + z, win ? BlockId::GlassLight : BlockId::Wood);
                }
        for (int x = 0; x < fw; ++x)
            for (int z = 0; z < fd; ++z)
                setBlock(ax + x, f + wallH + 1, az + z, BlockId::HayThatch);
        for (int x = 0; x < fw; ++x)
            setBlock(ax + x, f + wallH + 2, az + fd / 2, BlockId::RoofTerracotta);
        const int hcx = ax + fw - 1;
        const int hcz = az + fd - 1;
        for (int y = 1; y <= wallH + 3; ++y)
            setBlock(hcx, f + y, hcz, BlockId::MossBrick);
        addChimneySmoke(static_cast<float>(hcx) + 0.52f, static_cast<float>(f + wallH + 3), static_cast<float>(hcz) + 0.48f,
                        smokeSeed);
    };

    auto buildChapel = [&](int ax, int az, float smokeSeed) {
        constexpr int fw = 11;
        constexpr int fd = 9;
        constexpr int wallH = 5;
        int maxSy = -1;
        for (int x = 0; x < fw; ++x) {
            for (int z = 0; z < fd; ++z) {
                const int s = villageGroundY(ax + x, az + z);
                if (s < 0)
                    return;
                maxSy = std::max(maxSy, s);
            }
        }
        const int f = maxSy + 1;
        for (int x = 0; x < fw; ++x)
            for (int z = 0; z < fd; ++z) {
                const int localSy = villageGroundY(ax + x, az + z);
                if (localSy < 0)
                    continue;
                for (int yy = localSy + 1; yy < f; ++yy)
                    setBlock(ax + x, yy, az + z, BlockId::Marble);
            }
        for (int x = 0; x < fw; ++x)
            for (int z = 0; z < fd; ++z)
                setBlock(ax + x, f, az + z, BlockId::PathStone);
        for (int y = 1; y <= wallH; ++y)
            for (int x = 1; x < fw - 1; ++x)
                for (int z = 1; z < fd - 1; ++z)
                    setBlock(ax + x, f + y, az + z, BlockId::Air);
        for (int y = 1; y <= wallH; ++y)
            for (int x = 0; x < fw; ++x)
                for (int z = 0; z < fd; ++z) {
                    const bool edge = (x == 0 || x == fw - 1 || z == 0 || z == fd - 1);
                    if (!edge)
                        continue;
                    const int dx = fw / 2;
                    if (z == 0 && x == dx && y <= 2)
                        continue;
                    const bool win = (y == 3 || y == 4) && (x == 1 || x == fw - 2) && (z == 0 || z == fd - 1);
                    const bool pillar = (x == 0 || x == fw - 1) && (z == 2 || z == fd - 3);
                    setBlock(ax + x, f + y, az + z,
                             win ? BlockId::GlassDark : (pillar ? BlockId::MossStone : BlockId::MossBrick));
                }
        for (int x = 0; x < fw; ++x)
            for (int z = 0; z < fd; ++z)
                setBlock(ax + x, f + wallH + 1, az + z, BlockId::Slate);
        const int tx = ax + fw / 2 - 1;
        const int tz = az + fd / 2 - 1;
        for (int y = 1; y <= wallH + 6; ++y)
            for (int x = 0; x < 3; ++x)
                for (int z = 0; z < 3; ++z) {
                    const bool edge = (x == 0 || x == 2 || z == 0 || z == 2);
                    if (!edge && y < wallH + 5)
                        continue;
                    setBlock(tx + x, f + y, tz + z, edge ? BlockId::Slate : BlockId::Air);
                }
        for (int x = -1; x <= 3; ++x)
            setBlock(tx + x, f + wallH + 7, tz + 1, BlockId::Shingle);
        addChimneySmoke(static_cast<float>(tx + 1) + 0.5f, static_cast<float>(f + wallH + 7), static_cast<float>(tz + 1) + 0.5f,
                        smokeSeed);
        addScaledPropAt(static_cast<float>(tx + 1) + 0.5f, static_cast<float>(f + wallH + 9), static_cast<float>(tz + 1) + 0.5f,
                        0.9f, static_cast<int>(BlockId::Iron), 0.0f);
    };

    auto buildInn = [&](int ax, int az, float smokeSeed) {
        constexpr int fw = 12;
        constexpr int fd = 8;
        constexpr int wing = 6;
        constexpr int wallH = 4;
        int maxSy = -1;
        for (int x = 0; x < fw + wing; ++x) {
            for (int z = 0; z < fd; ++z) {
                const int s = villageGroundY(ax + x, az + z);
                if (s < 0)
                    return;
                maxSy = std::max(maxSy, s);
            }
        }
        const int f = maxSy + 1;
        auto stampPad = [&](int px, int pz, int w, int d, BlockId pad) {
            for (int x = 0; x < w; ++x)
                for (int z = 0; z < d; ++z) {
                    const int sy = villageGroundY(ax + px + x, az + pz + z);
                    if (sy < 0)
                        continue;
                    for (int yy = sy + 1; yy < f; ++yy)
                        setBlock(ax + px + x, yy, az + pz + z, BlockId::Cobble);
                    setBlock(ax + px + x, f, az + pz + z, pad);
                }
        };
        stampPad(0, 0, fw, fd, BlockId::Plank);
        stampPad(fw - 2, fd - 2, wing, wing, BlockId::Plank);
        for (int y = 1; y <= wallH; ++y) {
            for (int x = 0; x < fw; ++x) {
                for (int z = 0; z < fd; ++z) {
                    const bool edge = (x == 0 || x == fw - 1 || z == 0 || z == fd - 1);
                    if (!edge) {
                        setBlock(ax + x, f + y, az + z, BlockId::Air);
                        continue;
                    }
                    const bool door = (z == 0 && x == fw / 2 && y <= 2);
                    const bool win = (y == 2 || y == 3) && ((x == 1 || x == fw - 2) && (z == 0 || z == fd - 1));
                    setBlock(ax + x, f + y, az + z,
                             door ? BlockId::Air : (win ? BlockId::GlassLight : BlockId::Brick));
                }
            }
            for (int x = 0; x < wing; ++x) {
                for (int z = 0; z < wing; ++z) {
                    const int wx = fw - 2 + x;
                    const int wz = fd - 2 + z;
                    const bool edge = (x == 0 || x == wing - 1 || z == 0 || z == wing - 1);
                    if (!edge) {
                        setBlock(ax + wx, f + y, az + wz, BlockId::Air);
                        continue;
                    }
                    setBlock(ax + wx, f + y, az + wz, BlockId::DarkWood);
                }
            }
        }
        for (int x = 0; x < fw; ++x)
            for (int z = 0; z < fd; ++z)
                setBlock(ax + x, f + wallH + 1, az + z, BlockId::Shingle);
        for (int x = 0; x < wing; ++x)
            for (int z = 0; z < wing; ++z)
                setBlock(ax + fw - 2 + x, f + wallH + 1, az + fd - 2 + z, BlockId::HayThatch);
        addChimneySmoke(static_cast<float>(ax + fw - 2) + 0.5f, static_cast<float>(f + wallH + 2),
                        static_cast<float>(az + fd - 2) + 0.5f, smokeSeed);
    };

    auto buildWatchtower = [&](int ax, int az) {
        constexpr int r = 4;
        int maxSy = -1;
        for (int x = -r; x <= r; ++x) {
            for (int z = -r; z <= r; ++z) {
                if (x * x + z * z > r * r + 1)
                    continue;
                const int s = villageGroundY(ax + x, az + z);
                if (s < 0)
                    return;
                maxSy = std::max(maxSy, s);
            }
        }
        const int f = maxSy + 1;
        for (int x = -r; x <= r; ++x) {
            for (int z = -r; z <= r; ++z) {
                if (x * x + z * z > r * r + 1)
                    continue;
                const int sy = villageGroundY(ax + x, az + z);
                if (sy < 0)
                    continue;
                for (int yy = sy + 1; yy < f; ++yy)
                    setBlock(ax + x, yy, az + z, BlockId::Stone);
                setBlock(ax + x, f, az + z, BlockId::Cobble);
            }
        }
        for (int y = 1; y <= 11; ++y) {
            for (int x = -r; x <= r; ++x) {
                for (int z = -r; z <= r; ++z) {
                    const int d2 = x * x + z * z;
                    if (d2 > r * r)
                        continue;
                    const bool shell = (d2 >= (r - 1) * (r - 1));
                    if (!shell) {
                        setBlock(ax + x, f + y, az + z, BlockId::Air);
                        continue;
                    }
                    const bool slot = (y == 4 || y == 8) && (x == 0 || z == 0) && std::abs(x) + std::abs(z) == r - 1;
                    setBlock(ax + x, f + y, az + z, slot ? BlockId::GlassDark : BlockId::MossStone);
                }
            }
        }
        for (int x = -r; x <= r; ++x)
            for (int z = -r; z <= r; ++z)
                if (x * x + z * z <= r * r)
                    setBlock(ax + x, f + 12, az + z, BlockId::Slate);
        addScaledPropAt(static_cast<float>(ax) + 0.5f, static_cast<float>(f + 13), static_cast<float>(az) + 0.5f, 0.75f,
                        static_cast<int>(BlockId::Iron), static_cast<float>(seed % 360u));
    };

    const float scaledAvoid = hub ? 24.0f : 14.0f;
    auto addScaled = [this, anchorWx, anchorWz, scaledAvoid](float x, float z, float side, int tid) {
        addScaledProp(x, z, side, tid, 0.0f, anchorWx, anchorWz, scaledAvoid);
    };

    auto lampPick = [](uint32_t h) -> BlockId {
        switch (h % 4u) {
        case 0:
            return BlockId::LampWarm;
        case 1:
            return BlockId::LampWhite;
        case 2:
            return BlockId::LampCyan;
        default:
            return BlockId::LampMagenta;
        }
    };

    const int pcx = anchorWx;
    const int pcz = anchorWz;

    if (hub) {
        for (int wx = pcx - 10; wx <= pcx + 10; ++wx) {
            for (int wz = pcz - 10; wz <= pcz + 10; ++wz) {
                const int dx = wx - pcx;
                const int dz = wz - pcz;
                if (dx * dx + dz * dz > 92)
                    continue;
                const int sy = villageGroundY(wx, wz);
                if (sy < 0)
                    continue;
                setBlock(wx, sy - 1, wz, BlockId::Dirt);
                if (dx * dx + dz * dz < 48) {
                    /* 干塘：与周围同高，无水块 */
                    setBlock(wx, sy, wz, BlockId::PathStone);
                } else {
                    setBlock(wx, sy, wz, BlockId::Sand);
                }
            }
        }

        int bridgeH = -999;
        for (int wx = anchorWx - 9; wx <= anchorWx + 9; ++wx)
            bridgeH = std::max(bridgeH, villageGroundY(wx, pcz));
        if (bridgeH >= 0) {
            const int deckY = bridgeH + 3;
            for (int wx = anchorWx - 9; wx <= anchorWx + 9; ++wx)
                for (int dz = -2; dz <= 2; ++dz)
                    setBlock(wx, deckY, pcz + dz, BlockId::Wood);
            for (int wx = anchorWx - 9; wx <= anchorWx + 9; ++wx)
                for (int dz = -2; dz <= 2; ++dz)
                    setBlock(wx, deckY + 1, pcz + dz, BlockId::Wood);
        }

        for (int s = 0; s <= 52; ++s) {
            const float t = static_cast<float>(s) / 52.0f;
            const int wx = static_cast<int>(std::floor(static_cast<float>(anchorWx) - 22.0f + t * 42.0f));
            const int wz = static_cast<int>(std::floor(static_cast<float>(anchorWz) - 16.0f + t * 36.0f));
            for (int ox = -2; ox <= 2; ++ox)
                for (int oz = -2; oz <= 2; ++oz) {
                    const int sy = villageGroundY(wx + ox, wz + oz);
                    if (sy < 0)
                        continue;
                    const BlockId cur = getBlock(wx + ox, sy, wz + oz);
                    if (cur == BlockId::Water)
                        continue;
                    if (cur == BlockId::Grass || cur == BlockId::Dirt || cur == BlockId::Sand)
                        setBlock(wx + ox, sy, wz + oz, BlockId::PathStone);
                }
        }

        /* 主干马路：十字 PathStone，两侧略宽 */
        {
            auto stampSeg = [this, &villageGroundY](int xa, int za, int xb, int zb) {
                const int steps = std::max(std::abs(xb - xa), std::abs(zb - za));
                for (int s = 0; s <= steps; ++s) {
                    const float u = steps <= 0 ? 0.0f : static_cast<float>(s) / static_cast<float>(steps);
                    const int wx = xa + static_cast<int>(std::round((xb - xa) * u));
                    const int wz = za + static_cast<int>(std::round((zb - za) * u));
                    for (int ox = -1; ox <= 1; ++ox)
                        for (int oz = -1; oz <= 1; ++oz) {
                            const int sy = villageGroundY(wx + ox, wz + oz);
                            if (sy < 0)
                                continue;
                            const BlockId cur = getBlock(wx + ox, sy, wz + oz);
                            if (cur == BlockId::Water)
                                continue;
                            if (cur == BlockId::Grass || cur == BlockId::Dirt || cur == BlockId::Sand)
                                setBlock(wx + ox, sy, wz + oz, BlockId::PathStone);
                            if ((std::abs(ox) + std::abs(oz)) == 2 &&
                                (cur == BlockId::Grass || cur == BlockId::Dirt || cur == BlockId::Sand))
                                setBlock(wx + ox, sy, wz + oz, BlockId::Cobble);
                        }
                }
            };
            stampSeg(anchorWx - 78, anchorWz, anchorWx + 78, anchorWz);
            stampSeg(anchorWx, anchorWz - 68, anchorWx, anchorWz + 68);
        }

        for (int s = 0; s <= 52; s += 6) {
            const float t = static_cast<float>(s) / 52.0f;
            const int wx = static_cast<int>(std::floor(static_cast<float>(anchorWx) - 22.0f + t * 42.0f));
            const int wz = static_cast<int>(std::floor(static_cast<float>(anchorWz) - 16.0f + t * 36.0f));
            const int sy = villageGroundY(wx, wz);
            if (sy < 0)
                continue;
            setBlock(wx, sy + 1, wz, BlockId::Wood);
            setBlock(wx, sy + 2, wz, BlockId::Wood);
            setBlock(wx, sy + 3, wz, BlockId::LampWarm);
            setBlock(wx, sy + 4, wz, BlockId::MossBrick);
        }

        buildHouse(anchorWx - 16, anchorWz - 14, 7, 6, 3, 1.15f);
        buildHouse(anchorWx + 4, anchorWz - 10, 8, 7, 4, 2.4f);
        buildHouse(anchorWx - 20, anchorWz + 4, 5, 5, 2, 0.62f);
        buildHouse(anchorWx - 10, anchorWz + 10, 6, 5, 3, 1.55f);
        buildHouse(anchorWx + 24, anchorWz + 8, 9, 8, 4, static_cast<float>(seed) * 0.0016f);
        buildHouse(anchorWx - 8, anchorWz - 26, 10, 6, 5, static_cast<float>(seed) * 0.0022f);
        buildHouse(anchorWx + 10, anchorWz + 22, 7, 7, 3, static_cast<float>(seed) * 0.0019f);
        buildChapel(anchorWx + 20, anchorWz - 26, 3.15f);
        buildInn(anchorWx - 32, anchorWz - 4, 2.85f);
        buildWatchtower(anchorWx + 34, anchorWz + 22);

        for (int i = 0; i < 16; ++i) {
            const float a = static_cast<float>(i) * (6.2831853f / 16.0f);
            const float big = (i % 4 == 0) ? 2.35f : 0.72f;
            const int tid = (i % 4 == 0)   ? static_cast<int>(BlockId::MossStone)
                            : (i % 4 == 1) ? static_cast<int>(BlockId::Marble)
                            : (i % 4 == 2) ? static_cast<int>(BlockId::Slate)
                                           : static_cast<int>(BlockId::Cobble);
            addScaled(static_cast<float>(pcx) + 12.0f * std::cos(a), static_cast<float>(pcz) + 12.0f * std::sin(a), big, tid);
        }
        addScaled(static_cast<float>(pcx) + 7.0f, static_cast<float>(pcz) - 3.5f, 2.75f, static_cast<int>(BlockId::Marble));
        addScaled(static_cast<float>(pcx) - 6.5f, static_cast<float>(pcz) + 8.0f, 2.2f, static_cast<int>(BlockId::DarkWood));
        addScaled(static_cast<float>(pcx) - 10.0f, static_cast<float>(pcz) - 7.0f, 1.15f, static_cast<int>(BlockId::Iron));
        addScaled(static_cast<float>(pcx) + 2.5f, static_cast<float>(pcz) + 14.0f, 1.85f, static_cast<int>(BlockId::FlowerGrass));
        addScaled(static_cast<float>(pcx) + 26.0f, static_cast<float>(pcz) - 12.0f, 3.4f, static_cast<int>(BlockId::MossStone));
        addScaled(static_cast<float>(pcx) - 22.0f, static_cast<float>(pcz) + 16.0f, 2.6f, static_cast<int>(BlockId::Plank));

        placeCompositePrefabs(anchorWx, anchorWz, seed, true);
        if (sceneLighting_)
            game::SceneComposer::enrichHubRegion(*this, *sceneLighting_, anchorWx, anchorWz, seed);

        const int step = 12;
        for (int gx = -lampHalf; gx <= lampHalf; gx += step) {
            for (int gz = -lampHalf; gz <= lampHalf; gz += step) {
                const uint32_t h0 = static_cast<uint32_t>(gx * 502561 + gz * 928371 + anchorWx * 17 + anchorWz * 31);
                if ((h0 >> 5) % 5u < 2u)
                    continue;
                const int offx = static_cast<int>(h0 % 5u) - 2;
                const int offz = static_cast<int>((h0 >> 10) % 5u) - 2;
                const int wx = anchorWx + gx + offx;
                const int wz = anchorWz + gz + offz;
                const int ddx = wx - anchorWx;
                const int ddz = wz - anchorWz;
                if (ddx * ddx + ddz * ddz < 22 * 22)
                    continue;
                const int sy = villageGroundY(wx, wz);
                if (sy < 0)
                    continue;
                const BlockId g = getBlock(wx, sy, wz);
                if (g != BlockId::Grass && g != BlockId::Dirt && g != BlockId::Sand && g != BlockId::PathStone)
                    continue;
                if (getBlock(wx, sy + 1, wz) != BlockId::Air)
                    continue;
                setBlock(wx, sy + 1, wz, BlockId::Wood);
                setBlock(wx, sy + 2, wz, BlockId::Wood);
                setBlock(wx, sy + 3, wz, lampPick(h0));
                setBlock(wx, sy + 4, wz, BlockId::MossBrick);
            }
        }
    } else {
        const int ox0 = static_cast<int>(seed % 5u) - 2;
        const int oz0 = static_cast<int>((seed >> 4) % 5u) - 2;
        for (int wx = pcx - 7 + ox0; wx <= pcx + 7 + ox0; ++wx) {
            for (int wz = pcz - 7 + oz0; wz <= pcz + 7 + oz0; ++wz) {
                const int dx = wx - (pcx + ox0);
                const int dz = wz - (pcz + oz0);
                if (dx * dx + dz * dz > 34)
                    continue;
                const int sy = villageGroundY(wx, wz);
                if (sy < 0)
                    continue;
                setBlock(wx, sy - 1, wz, BlockId::Dirt);
                if (dx * dx + dz * dz < 14) {
                    setBlock(wx, sy, wz, BlockId::PathStone);
                } else {
                    setBlock(wx, sy, wz, BlockId::Sand);
                }
            }
        }

        int bridgeH = -999;
        for (int wx = anchorWx - 5 + ox0; wx <= anchorWx + 5 + ox0; ++wx)
            bridgeH = std::max(bridgeH, villageGroundY(wx, pcz + oz0));
        if (bridgeH >= 0) {
            const int deckY = bridgeH + 3;
            for (int wx = anchorWx - 5 + ox0; wx <= anchorWx + 5 + ox0; ++wx)
                for (int dz = -2; dz <= 2; ++dz)
                    setBlock(wx, deckY, pcz + oz0 + dz, BlockId::Wood);
            for (int wx = anchorWx - 5 + ox0; wx <= anchorWx + 5 + ox0; ++wx)
                for (int dz = -2; dz <= 2; ++dz)
                    setBlock(wx, deckY + 1, pcz + oz0 + dz, BlockId::Wood);
        }

        const int pathN = 24;
        for (int s = 0; s <= pathN; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(pathN);
            const float ang = static_cast<float>(seed % 360u) * 0.017453292f;
            const float ca = std::cos(ang);
            const float sa = std::sin(ang);
            const float bx = -10.0f + t * 22.0f;
            const float bz = -8.0f + t * 18.0f;
            const int wx = static_cast<int>(std::floor(static_cast<float>(anchorWx) + bx * ca - bz * sa));
            const int wz = static_cast<int>(std::floor(static_cast<float>(anchorWz) + bx * sa + bz * ca));
            for (int ox = -2; ox <= 2; ++ox)
                for (int oz = -2; oz <= 2; ++oz) {
                    const int sy = villageGroundY(wx + ox, wz + oz);
                    if (sy < 0)
                        continue;
                    const BlockId cur = getBlock(wx + ox, sy, wz + oz);
                    if (cur == BlockId::Water)
                        continue;
                    if (cur == BlockId::Grass || cur == BlockId::Dirt || cur == BlockId::Sand)
                        setBlock(wx + ox, sy, wz + oz, BlockId::PathStone);
                }
        }

        for (int s = 0; s <= pathN; s += 6) {
            const float t = static_cast<float>(s) / static_cast<float>(pathN);
            const float ang = static_cast<float>(seed % 360u) * 0.017453292f;
            const float ca = std::cos(ang);
            const float sa = std::sin(ang);
            const float bx = -10.0f + t * 22.0f;
            const float bz = -8.0f + t * 18.0f;
            const int wx = static_cast<int>(std::floor(static_cast<float>(anchorWx) + bx * ca - bz * sa));
            const int wz = static_cast<int>(std::floor(static_cast<float>(anchorWz) + bx * sa + bz * ca));
            const int sy = villageGroundY(wx, wz);
            if (sy < 0)
                continue;
            const uint32_t h0 = seed + static_cast<uint32_t>(s) * 131u;
            setBlock(wx, sy + 1, wz, BlockId::Wood);
            setBlock(wx, sy + 2, wz, BlockId::Wood);
            setBlock(wx, sy + 3, wz, lampPick(h0));
            setBlock(wx, sy + 4, wz, BlockId::MossBrick);
        }

        const int hfx = -9 + static_cast<int>((seed >> 2) % 7u);
        const int hfz = -8 + static_cast<int>((seed >> 6) % 7u);
        buildHouse(anchorWx + hfx, anchorWz + hfz, 6, 5, 2, static_cast<float>(seed) * 0.0013f);

        for (int i = 0; i < 6; ++i) {
            const float a = static_cast<float>(i) * (6.2831853f / 6.0f);
            const float side = (i % 3 == 0) ? 1.55f : 0.62f;
            const int tid = (i % 3 == 0)   ? static_cast<int>(BlockId::Stone)
                            : (i % 3 == 1) ? static_cast<int>(BlockId::MossStone)
                                           : static_cast<int>(BlockId::Plank);
            addScaled(static_cast<float>(pcx) + 9.0f * std::cos(a), static_cast<float>(pcz) + 9.0f * std::sin(a), side, tid);
        }
        if ((seed & 3u) == 0u)
            buildWatchtower(anchorWx + 10 + static_cast<int>((seed >> 4) % 5u), anchorWz + 12);
        if ((seed & 7u) == 3u)
            buildChapel(anchorWx - 14, anchorWz + 10, static_cast<float>(seed) * 0.0011f);
        placeCompositePrefabs(anchorWx, anchorWz, seed, false);
        if (sceneLighting_)
            game::SceneComposer::enrichRegionalCluster(*this, *sceneLighting_, anchorWx, anchorWz, seed);
    }
}

void VoxelWorld::decorateChunk(Chunk& ch) {
    const int ox = ch.cx * kChunkSize;
    const int oy = ch.cy * kChunkSize;
    const int oz = ch.cz * kChunkSize;
    const int seaI = static_cast<int>(std::floor(kSeaLevel));

    bool occ[16][16] = {};
    auto markRange = [&](int x0, int x1, int z0, int z1) {
        for (int zz = z0; zz <= z1; ++zz)
            for (int xx = x0; xx <= x1; ++xx)
                if (xx >= 0 && xx < kChunkSize && zz >= 0 && zz < kChunkSize)
                    occ[xx][zz] = true;
    };

    auto surfaceGrass = [&](int lx, int lz, int& outLy) -> bool {
        outLy = topSolidLy(ch, lx, lz);
        if (outLy < 0)
            return false;
        const BlockId top = ch.getLocal(lx, outLy, lz);
        if (top != BlockId::Grass)
            return false;
        const int hi = oy + outLy;
        if (hi <= seaI + 2)
            return false;
        return true;
    };

    auto flatPlateau = [&](int cx, int cz, int ly0, int r) -> bool {
        for (int dz = -r; dz <= r; ++dz) {
            for (int dx = -r; dx <= r; ++dx) {
                const int lx = cx + dx;
                const int lz = cz + dz;
                if (lx < 0 || lx >= kChunkSize || lz < 0 || lz >= kChunkSize)
                    return false;
                const int t = topSolidLy(ch, lx, lz);
                if (t != ly0 || ch.getLocal(lx, t, lz) != BlockId::Grass)
                    return false;
            }
        }
        return true;
    };

    /* 小屋：5×5 地坪 + 中空木墙 + 顶 */
    {
        const int hx = 8, hz = 8;
        int ly0 = 0;
        if (!occ[hx][hz] && surfaceGrass(hx, hz, ly0) && ly0 + 6 < kChunkSize && flatPlateau(hx, hz, ly0, 2) &&
            hash12(static_cast<float>(ox + hx), static_cast<float>(oz + hz)) < 0.018f) {
            for (int lz = 6; lz <= 10; ++lz) {
                for (int lx = 6; lx <= 10; ++lx)
                    ch.setLocal(lx, ly0 + 1, lz, BlockId::Cobble);
            }
            for (int h = 2; h <= 4; ++h) {
                const int y = ly0 + h;
                for (int lz = 6; lz <= 10; ++lz) {
                    for (int lx = 6; lx <= 10; ++lx) {
                        const bool edge = (lx == 6 || lx == 10 || lz == 6 || lz == 10);
                        if (!edge) {
                            ch.setLocal(lx, y, lz, BlockId::Air);
                            continue;
                        }
                        const bool win = (h == 3) && (((lx == 8 && lz == 6) || (lx == 8 && lz == 10) || (lx == 6 && lz == 8) || (lx == 10 && lz == 8)));
                        const float wg = hash12(static_cast<float>(ox + lx * 3 + y), static_cast<float>(oz + lz + 1));
                        const BlockId winB =
                            wg < 0.34f ? BlockId::GlassLight : (wg < 0.67f ? BlockId::GlassDark : BlockId::Glass);
                        ch.setLocal(lx, y, lz, win ? winB : BlockId::Wood);
                    }
                }
            }
            for (int lz = 6; lz <= 10; ++lz) {
                for (int lx = 6; lx <= 10; ++lx)
                    ch.setLocal(lx, ly0 + 5, lz, BlockId::Shingle);
            }
            markRange(4, 12, 4, 12);
        }
    }

    /* 路边神龛：大理石基座 + 缩放铁饰（登记到世界层） */
    {
        const int sx = 12, sz = 4;
        int ly0 = 0;
        if (!occ[sx][sz] && surfaceGrass(sx, sz, ly0) && ly0 + 3 < kChunkSize &&
            hash12(static_cast<float>(ox + sx + 7), static_cast<float>(oz + sz)) < 0.012f) {
            ch.setLocal(sx, ly0 + 1, sz, BlockId::Marble);
            ch.setLocal(sx, ly0 + 2, sz, BlockId::MossStone);
            addScaledPropAt(static_cast<float>(ox + sx) + 0.5f, static_cast<float>(oy + ly0 + 3), static_cast<float>(oz + sz) + 0.5f,
                            0.65f, static_cast<int>(BlockId::Iron), hash12(static_cast<float>(ox), static_cast<float>(oz)) * 360.0f);
            markRange(10, 14, 2, 6);
        }
    }

    /* 树木：木柱 + 顶部草叶 */
    {
        const int tx = 4, tz = 12;
        int ly0 = 0;
        if (!occ[tx][tz] && surfaceGrass(tx, tz, ly0) && ly0 + 4 < kChunkSize && flatPlateau(tx, tz, ly0, 1) &&
            hash12(static_cast<float>(ox + tx + 19), static_cast<float>(oz + tz)) < 0.045f) {
            for (int k = 1; k <= 3; ++k)
                ch.setLocal(tx, ly0 + k, tz, BlockId::Wood);
            const int yc = ly0 + 4;
            ch.setLocal(tx, yc, tz, BlockId::Grass);
            if (tx + 1 < kChunkSize)
                ch.setLocal(tx + 1, yc, tz, BlockId::Grass);
            if (tx - 1 >= 0)
                ch.setLocal(tx - 1, yc, tz, BlockId::Grass);
            if (tz + 1 < kChunkSize)
                ch.setLocal(tx, yc, tz + 1, BlockId::Grass);
            if (tz - 1 >= 0)
                ch.setLocal(tx, yc, tz - 1, BlockId::Grass);
            markRange(2, 6, 10, 14);
        }
    }

    /* 灌木 / 高草：小块草堆 */
    for (int lz = 1; lz < kChunkSize - 1; lz += 3) {
        for (int lx = 1; lx < kChunkSize - 1; lx += 3) {
            if (occ[lx][lz])
                continue;
            int ly0 = 0;
            if (!surfaceGrass(lx, lz, ly0))
                continue;
            const float u = hash12(static_cast<float>(ox + lx * 2), static_cast<float>(oz + lz * 3 + 11));
            if (u > 0.038f)
                continue;
            if (ly0 + 2 >= kChunkSize)
                continue;
            ch.setLocal(lx, ly0 + 1, lz, BlockId::Grass);
            if (u < 0.018f && ly0 + 2 < kChunkSize)
                ch.setLocal(lx, ly0 + 2, lz, BlockId::Grass);
        }
    }

    /* 旗帜：木杆 + 登记动态布面 */
    {
        const int fx = 2, fz = 13;
        int ly0 = 0;
        if (!occ[fx][fz] && surfaceGrass(fx, fz, ly0) && ly0 + 2 < kChunkSize &&
            hash12(static_cast<float>(ch.cx * 131 + ch.cy), static_cast<float>(ch.cz * 57)) < 0.14f) {
            ch.setLocal(fx, ly0 + 1, fz, BlockId::Wood);
            ch.setLocal(fx, ly0 + 2, fz, BlockId::Wood);
            SceneryFlag sf;
            sf.poleX = static_cast<float>(ox + fx) + 0.58f;
            sf.poleZ = static_cast<float>(oz + fz) + 0.5f;
            sf.clothAnchorY = static_cast<float>(oy + ly0 + 3);
            sf.yawDeg = hash12(static_cast<float>(ox), static_cast<float>(oz + 3)) * 360.0f;
            sf.colorSeed = static_cast<int>(hash12(static_cast<float>(ch.cx), static_cast<float>(ch.cz)) * 2.99f);
            sceneryFlags_.push_back(sf);
            markRange(0, 5, 11, 15);
        }
    }
}

bool VoxelWorld::placeFromVoxDat(const char* path, int originWx, int originWy, int originWz, BlockId material) {
    (void)originWy;
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    char magic[4];
    in.read(magic, 4);
    if (!in || std::memcmp(magic, "HVX1", 4) != 0)
        return false;
    uint32_t n = 0;
    in.read(reinterpret_cast<char*>(&n), 4);
    if (!in || n == 0)
        return false;
    constexpr int kGrid = 48;
    struct Cell {
        unsigned char x, y, z;
    };
    std::vector<Cell> cells;
    cells.reserve(static_cast<size_t>(n));
    int minIy = kGrid;
    int minIx = kGrid, maxIx = -1, minIz = kGrid, maxIz = -1;
    for (uint32_t i = 0; i < n; ++i) {
        unsigned char ijk[3];
        in.read(reinterpret_cast<char*>(ijk), 3);
        if (!in)
            return false;
        if (ijk[0] >= kGrid || ijk[1] >= kGrid || ijk[2] >= kGrid)
            continue;
        cells.push_back(Cell{ijk[0], ijk[1], ijk[2]});
        minIy = std::min(minIy, static_cast<int>(ijk[1]));
        minIx = std::min(minIx, static_cast<int>(ijk[0]));
        maxIx = std::max(maxIx, static_cast<int>(ijk[0]));
        minIz = std::min(minIz, static_cast<int>(ijk[2]));
        maxIz = std::max(maxIz, static_cast<int>(ijk[2]));
    }
    if (cells.empty() || minIy >= kGrid || maxIx < 0)
        return false;
    int terrainTop = -1;
    for (int ix = minIx; ix <= maxIx; ++ix) {
        for (int iz = minIz; iz <= maxIz; ++iz) {
            const int wx = originWx + ix;
            const int wz = originWz + iz;
            terrainTop = std::max(terrainTop, terrainHeight(wx, wz));
        }
    }
    if (terrainTop < 0)
        return false;
    const int wy0 = terrainTop + 1 - minIy;

    for (const Cell& c : cells)
        setBlock(originWx + static_cast<int>(c.x), wy0 + static_cast<int>(c.y), originWz + static_cast<int>(c.z), material);
    return true;
}

bool VoxelWorld::raycastBlock(glm::vec3 origin, glm::vec3 dir, float maxDist, glm::ivec3& outBlock,
                              glm::ivec3& outNormal) const {
    glm::vec3 d = dir;
    const float len = glm::length(d);
    if (len < 1e-6f)
        return false;
    d /= len;

    const glm::ivec3 map(static_cast<int>(std::floor(origin.x)), static_cast<int>(std::floor(origin.y)),
                         static_cast<int>(std::floor(origin.z)));

    const glm::ivec3 step(d.x >= 0.0f ? 1 : -1, d.y >= 0.0f ? 1 : -1, d.z >= 0.0f ? 1 : -1);

    auto safeInv = [](float v) -> float {
        const float a = std::fabs(v);
        return a > 1e-8f ? (1.0f / v) : ((v >= 0.0f ? 1.0f : -1.0f) * FLT_MAX);
    };
    const glm::vec3 invDir(safeInv(d.x), safeInv(d.y), safeInv(d.z));
    const glm::vec3 deltaDist(std::fabs(invDir.x), std::fabs(invDir.y), std::fabs(invDir.z));

    glm::vec3 tMax;
    if (step.x > 0)
        tMax.x = (static_cast<float>(map.x) + 1.0f - origin.x) * invDir.x;
    else
        tMax.x = (static_cast<float>(map.x) - origin.x) * invDir.x;
    if (step.y > 0)
        tMax.y = (static_cast<float>(map.y) + 1.0f - origin.y) * invDir.y;
    else
        tMax.y = (static_cast<float>(map.y) - origin.y) * invDir.y;
    if (step.z > 0)
        tMax.z = (static_cast<float>(map.z) + 1.0f - origin.z) * invDir.z;
    else
        tMax.z = (static_cast<float>(map.z) - origin.z) * invDir.z;

    if (d.x == 0.0f)
        tMax.x = FLT_MAX;
    if (d.y == 0.0f)
        tMax.y = FLT_MAX;
    if (d.z == 0.0f)
        tMax.z = FLT_MAX;

    glm::ivec3 cell = map;
    glm::ivec3 norm(0, 0, 0);

    for (int i = 0; i < 4096; ++i) {
        BlockId b = getBlock(cell.x, cell.y, cell.z);
        if (isSolid(b)) {
            outBlock = cell;
            outNormal = norm;
            return true;
        }

        const float tHit = std::min(tMax.x, std::min(tMax.y, tMax.z));
        if (tHit > maxDist)
            return false;

        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                tMax.x += deltaDist.x;
                cell.x += step.x;
                norm = glm::ivec3(-step.x, 0, 0);
            } else {
                tMax.z += deltaDist.z;
                cell.z += step.z;
                norm = glm::ivec3(0, 0, -step.z);
            }
        } else {
            if (tMax.y < tMax.z) {
                tMax.y += deltaDist.y;
                cell.y += step.y;
                norm = glm::ivec3(0, -step.y, 0);
            } else {
                tMax.z += deltaDist.z;
                cell.z += step.z;
                norm = glm::ivec3(0, 0, -step.z);
            }
        }
    }
    return false;
}

} // namespace voxel
