#include "game/meshcraft/MeshCraftWorld.h"

#include "game/meshcraft/BuildingGeom.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace meshcraft {

bool MeshCraftWorld::build(int terrainResolution, uint32_t seed) {
    if (!terrain_.build(terrainResolution, seed))
        return false;
    seedScene(seed);
    return true;
}

glm::ivec2 MeshCraftWorld::findHamletSite() const {
    int bestGx = 0;
    int bestGz = 0;
    float bestScore = 1e9f;

    for (int gz = -24; gz <= 24; ++gz) {
        for (int gx = -24; gx <= 24; ++gx) {
            float hMin = 1e9f;
            float hMax = -1e9f;
            float river = 0.0f;
            for (int dz = -2; dz <= 2; ++dz) {
                for (int dx = -2; dx <= 2; ++dx) {
                    const float wx = (static_cast<float>(gx + dx) + 0.5f) * MeshBuildingWorld::kGrid;
                    const float wz = (static_cast<float>(gz + dz) + 0.5f) * MeshBuildingWorld::kGrid;
                    const float h = terrain_.sampleHeight(wx, wz);
                    hMin = std::min(hMin, h);
                    hMax = std::max(hMax, h);
                    river = std::max(river, terrain_.sampleRiverMask(wx, wz));
                }
            }
            if (river > 0.35f)
                continue;
            const float slope = hMax - hMin;
            const float dist = std::sqrt(static_cast<float>(gx * gx + gz * gz));
            const float score = slope * 4.0f + river * 80.0f + dist * 0.15f;
            if (score < bestScore) {
                bestScore = score;
                bestGx = gx;
                bestGz = gz;
            }
        }
    }
    return {bestGx, bestGz};
}

void MeshCraftWorld::spawnRiverBridges(uint32_t seed) {
    const float s = static_cast<float>(seed) * 0.01f;
    const float half = terrain_.sizeXZ() * 0.5f;
    const float wl = terrain_.waterLevel();

    struct BridgeSpec {
        float ax, az, bx, bz;
    };
    const BridgeSpec specs[] = {
        {-half * 0.75f, -12.0f + std::sin(s) * 8.0f, half * 0.75f, 16.0f + std::cos(s) * 6.0f},
        {-28.0f + std::cos(s) * 6.0f, -half * 0.7f, 32.0f, half * 0.72f},
    };

    for (const BridgeSpec& spec : specs) {
        const int samples = 16;
        int waterSamples = 0;
        for (int i = 0; i <= samples; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(samples);
            const float x = glm::mix(spec.ax, spec.bx, t);
            const float z = glm::mix(spec.az, spec.bz, t);
            if (terrain_.sampleHeight(x, z) < wl + 0.5f)
                ++waterSamples;
        }
        if (waterSamples >= 4)
            buildings_.spawnBridge(terrain_, spec.ax, spec.az, spec.bx, spec.bz);
    }
}

void MeshCraftWorld::seedScene(uint32_t seed) {
    buildings_.clear();
    hamletGrid_ = findHamletSite();
    buildings_.seedStarterHamlet(terrain_, hamletGrid_.x, hamletGrid_.y);
    spawnRiverBridges(seed);

    const int manorGx = hamletGrid_.x - 14;
    const int manorGz = hamletGrid_.y - 12;
    const float mwx = (static_cast<float>(manorGx) + 2.0f) * MeshBuildingWorld::kGrid;
    const float mwz = (static_cast<float>(manorGz) + 1.5f) * MeshBuildingWorld::kGrid;
    if (terrain_.sampleRiverMask(mwx, mwz) < 0.3f)
        buildings_.spawnManor(terrain_, manorGx, manorGz);
}

glm::vec3 MeshCraftWorld::spawnEyePosition() const {
    const float wx = (static_cast<float>(hamletGrid_.x) + 0.5f) * MeshBuildingWorld::kGrid;
    const float wz = (static_cast<float>(hamletGrid_.y) + 0.5f) * MeshBuildingWorld::kGrid;
    const float ground = terrain_.sampleHeight(wx, wz);
    return glm::vec3(wx, ground + 3.5f, wz + 18.0f);
}

WorldPickResult MeshCraftWorld::pick(const glm::vec3& eye, const glm::vec3& dir, float maxDist) const {
    WorldPickResult r;
    glm::vec3 d = dir;
    const float len = glm::length(d);
    if (len < 1e-6f)
        return r;
    d /= len;

    glm::vec3 tHit, tN(0.0f, 1.0f, 0.0f);
    float tDist = maxDist;
    const bool hitTerrain = terrain_.raycast(eye, d, tDist, tHit, tN);

    glm::vec3 bHit, bN;
    float bDist = maxDist;
    const bool hitBuild = buildings_.raycastSolid(eye, d, bDist, bHit, &bN);

    if (hitBuild && (!hitTerrain || bDist < tDist)) {
        r.hit = true;
        r.hitBuilding = true;
        r.position = bHit;
        r.normal = bN;
        return r;
    }
    if (hitTerrain) {
        r.hit = true;
        r.hitBuilding = false;
        r.position = tHit;
        r.normal = tN;
        return r;
    }
    if (hitBuild) {
        r.hit = true;
        r.hitBuilding = true;
        r.position = bHit;
        r.normal = bN;
    }
    return r;
}

bool MeshCraftWorld::tryPlace(BuildingKind kind, const glm::vec3& eye, const glm::vec3& dir, float maxDist) {
    const WorldPickResult pr = pick(eye, dir, maxDist);
    if (!pr.hit)
        return false;

    glm::ivec3 cell;
    int layer = 0;
    buildings_.snapRayHitToCell(terrain_, pr.position, pr.normal, cell, layer);
    return buildings_.place(terrain_, cell.x, cell.z, layer, kind);
}

bool MeshCraftWorld::tryRemove(const glm::vec3& eye, const glm::vec3& dir, float maxDist) {
    const WorldPickResult pr = pick(eye, dir, maxDist);
    if (!pr.hit || !pr.hitBuilding)
        return false;
    return buildings_.removeAt(pr.position, 1.8f);
}

} // namespace meshcraft
