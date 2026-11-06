#include "game/meshcraft/MeshBuildingWorld.h"

#include "game/meshcraft/BuildingGeom.h"

#include <algorithm>
#include <cmath>

namespace meshcraft {

void MeshBuildingWorld::snapRayHitToCell(const TerrainMesh& terrain, const glm::vec3& hit, const glm::vec3& normal,
                                         glm::ivec3& outCell, int& outLayer) const {
    const float nx = normal.x;
    const float ny = normal.y;
    const float nz = normal.z;
    glm::vec3 place = hit;
    if (ny > 0.45f)
        place += glm::vec3(0.0f, 0.05f, 0.0f);
    else if (std::abs(nx) > std::abs(nz))
        place += glm::vec3(std::copysign(0.08f, nx), 0.0f, 0.0f);
    else
        place += glm::vec3(0.0f, 0.0f, std::copysign(0.08f, nz));

    outCell.x = static_cast<int>(std::floor(place.x / kGrid));
    outCell.z = static_cast<int>(std::floor(place.z / kGrid));
    const float wx = (static_cast<float>(outCell.x) + 0.5f) * kGrid;
    const float wz = (static_cast<float>(outCell.z) + 0.5f) * kGrid;
    const float ground = terrain.sampleHeight(wx, wz);
    outLayer = static_cast<int>(std::floor((place.y - ground + 0.12f) / kFloorH));
    if (outLayer < 0)
        outLayer = 0;
}

glm::vec3 MeshBuildingWorld::cellCenterWorld(const TerrainMesh& terrain, int gx, int gz, int layer) const {
    const float wx = (static_cast<float>(gx) + 0.5f) * kGrid;
    const float wz = (static_cast<float>(gz) + 0.5f) * kGrid;
    const float ground = terrain.sampleHeight(wx, wz);
    const float stackBase = ground + static_cast<float>(layer) * kFloorH;
    return glm::vec3(wx, stackBase + kFloorH * 0.5f, wz);
}

BuildingInstance MeshBuildingWorld::makeInstance(const TerrainMesh& terrain, int gx, int gz, int layer,
                                                 BuildingKind kind) const {
    BuildingInstance bi;
    bi.kind = kind;
    bi.center = cellCenterWorld(terrain, gx, gz, layer);
    bi.yawDeg = 0.0f;
    switch (kind) {
    case BuildingKind::Foundation:
        bi.halfExtents = glm::vec3(kGrid * 0.48f, 0.18f, kGrid * 0.48f);
        bi.center.y = terrain.sampleHeight(bi.center.x, bi.center.z) + 0.18f;
        break;
    case BuildingKind::Wall:
        bi.halfExtents = glm::vec3(kGrid * 0.48f, kFloorH * 0.48f, 0.22f);
        break;
    case BuildingKind::WallWindow:
        bi.halfExtents = glm::vec3(kGrid * 0.48f, kFloorH * 0.48f, 0.18f);
        break;
    case BuildingKind::Pillar:
        bi.halfExtents = glm::vec3(0.35f, kFloorH * 0.5f, 0.35f);
        break;
    case BuildingKind::Roof:
        bi.halfExtents = glm::vec3(kGrid * 0.52f, 0.35f, kGrid * 0.52f);
        bi.center.y += kFloorH * 0.45f;
        break;
    case BuildingKind::Stairs:
        bi.halfExtents = glm::vec3(kGrid * 0.5f, kFloorH * 0.25f, kGrid * 0.5f);
        break;
    case BuildingKind::BridgeDeck:
        bi.halfExtents = glm::vec3(kGrid * 0.95f, 0.42f, kGrid * 0.42f);
        break;
    case BuildingKind::BridgePier:
        bi.halfExtents = glm::vec3(0.85f, kFloorH * 1.1f, 0.85f);
        break;
    case BuildingKind::BridgeRail:
        bi.halfExtents = glm::vec3(kGrid * 0.92f, 0.28f, 0.14f);
        break;
    case BuildingKind::ManorWall:
        bi.halfExtents = glm::vec3(kGrid * 0.48f, kFloorH * 0.5f, kGrid * 0.48f);
        break;
    case BuildingKind::ManorRoof:
        bi.halfExtents = glm::vec3(kGrid * 0.55f, 0.45f, kGrid * 0.55f);
        bi.center.y += kFloorH * 0.5f;
        break;
    case BuildingKind::ManorTower:
        bi.halfExtents = glm::vec3(1.1f, kFloorH * 1.35f, 1.1f);
        break;
    default:
        break;
    }
    return bi;
}

void MeshBuildingWorld::addInstance(const BuildingInstance& bi) {
    instances_.push_back(bi);
}

int MeshBuildingWorld::findAt(int gx, int gz, int layer) const {
    for (size_t i = 0; i < instances_.size(); ++i) {
        const BuildingInstance& b = instances_[i];
        const int bx = static_cast<int>(std::floor(b.center.x / kGrid));
        const int bz = static_cast<int>(std::floor(b.center.z / kGrid));
        const int bl = static_cast<int>(std::floor((b.center.y - kFloorH * 0.25f) / kFloorH));
        if (bx == gx && bz == gz && bl == layer)
            return static_cast<int>(i);
    }
    return -1;
}

bool MeshBuildingWorld::canPlace(const TerrainMesh& terrain, int gx, int gz, int layer, BuildingKind kind) const {
    if (findAt(gx, gz, layer) >= 0)
        return false;
    const glm::vec3 c = cellCenterWorld(terrain, gx, gz, layer);
    const glm::vec2 o = terrain.originXZ();
    const float sz = terrain.sizeXZ();
    if (c.x < o.x + 2.0f || c.z < o.y + 2.0f || c.x > o.x + sz - 2.0f || c.z > o.y + sz - 2.0f)
        return false;
    const float wx = (static_cast<float>(gx) + 0.5f) * kGrid;
    const float wz = (static_cast<float>(gz) + 0.5f) * kGrid;
    if (terrain.sampleRiverMask(wx, wz) > 0.55f)
        return false;
    (void)kind;
    (void)layer;
    (void)terrain;
    return true;
}

bool MeshBuildingWorld::place(const TerrainMesh& terrain, int gx, int gz, int layer, BuildingKind kind) {
    if (!canPlace(terrain, gx, gz, layer, kind))
        return false;
    instances_.push_back(makeInstance(terrain, gx, gz, layer, kind));
    return true;
}

bool MeshBuildingWorld::removeAt(const glm::vec3& worldPos, float radius) {
    const float r2 = radius * radius;
    bool removed = false;
    for (size_t i = 0; i < instances_.size();) {
        if (glm::dot(instances_[i].center - worldPos, instances_[i].center - worldPos) <= r2) {
            instances_.erase(instances_.begin() + static_cast<std::ptrdiff_t>(i));
            removed = true;
        } else {
            ++i;
        }
    }
    return removed;
}

bool MeshBuildingWorld::raycastSolid(const glm::vec3& origin, const glm::vec3& dir, float& maxDist, glm::vec3& hit,
                                     glm::vec3* outNormal, glm::ivec3* outCell) const {
    float bestT = maxDist;
    bool found = false;
    glm::vec3 bestN(0.0f, 1.0f, 0.0f);
    for (const BuildingInstance& b : instances_) {
        const OrientedBox box = instanceBox(b.center, b.halfExtents, b.yawDeg);
        float t = maxDist;
        glm::vec3 n;
        if (!box.raycast(origin, dir, maxDist, t, n))
            continue;
        if (t < bestT) {
            bestT = t;
            bestN = n;
            hit = origin + dir * t;
            found = true;
        }
    }
    if (found) {
        maxDist = bestT;
        if (outNormal)
            *outNormal = bestN;
        if (outCell) {
            outCell->x = static_cast<int>(std::floor(hit.x / kGrid));
            outCell->z = static_cast<int>(std::floor(hit.z / kGrid));
            outCell->y = static_cast<int>(std::floor(hit.y / kFloorH));
        }
    }
    return found;
}

bool MeshBuildingWorld::collidesAabb(const glm::vec3& pmin, const glm::vec3& pmax) const {
    for (const BuildingInstance& b : instances_) {
        const OrientedBox box = instanceBox(b.center, b.halfExtents, b.yawDeg);
        const glm::vec3 wMin = box.worldAabbMin();
        const glm::vec3 wMax = box.worldAabbMax();
        if (pmin.x < wMax.x && pmax.x > wMin.x && pmin.y < wMax.y && pmax.y > wMin.y && pmin.z < wMax.z &&
            pmax.z > wMin.z)
            return true;
    }
    return false;
}

void MeshBuildingWorld::seedStarterHamlet(const TerrainMesh& terrain, int centerGx, int centerGz) {
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            place(terrain, centerGx + dx, centerGz + dz, 0, BuildingKind::Foundation);
        }
    }
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            const bool edge = std::abs(dx) == 2 || std::abs(dz) == 2;
            if (!edge)
                continue;
            const bool door = dz == 2 && dx == 0;
            place(terrain, centerGx + dx, centerGz + dz, 1,
                  door ? BuildingKind::WallWindow : BuildingKind::Wall);
        }
    }
    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            place(terrain, centerGx + dx, centerGz + dz, 2, BuildingKind::Roof);
        }
    }
    for (int i = 0; i < 4; ++i) {
        const int px = centerGx + (i < 2 ? -3 : 3);
        const int pz = centerGz + (i % 2 == 0 ? -3 : 3);
        place(terrain, px, pz, 0, BuildingKind::Pillar);
        place(terrain, px, pz, 1, BuildingKind::Pillar);
    }
}

void MeshBuildingWorld::spawnBridge(const TerrainMesh& terrain, float ax, float az, float bx, float bz) {
    const glm::vec2 a2(ax, az);
    const glm::vec2 b2(bx, bz);
    const glm::vec2 ab2 = b2 - a2;
    const float len = glm::length(ab2);
    if (len < kGrid)
        return;

    const int samples = std::max(8, static_cast<int>(len / 3.0f));
    bool crossesWater = false;
    float deckY = terrain.waterLevel() + 1.15f;
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const float x = glm::mix(ax, bx, t);
        const float z = glm::mix(az, bz, t);
        const float ground = terrain.sampleHeight(x, z);
        if (ground < terrain.waterLevel() + 0.45f)
            crossesWater = true;
        deckY = std::max(deckY, ground + 0.55f);
    }
    if (!crossesWater)
        return;

    const float yaw = glm::degrees(std::atan2(ab2.y, ab2.x));
    const glm::vec3 ab3(bx - ax, 0.0f, bz - az);
    const glm::vec3 mid((ax + bx) * 0.5f, deckY, (az + bz) * 0.5f);
    const glm::vec3 perp = glm::normalize(glm::vec3(-ab3.z, 0.0f, ab3.x));
    const float deckHalfY = 0.45f;

    BuildingInstance deck;
    deck.kind = BuildingKind::BridgeDeck;
    deck.center = mid;
    deck.halfExtents = glm::vec3(len * 0.5f + kGrid * 0.15f, deckHalfY, kGrid * 0.58f);
    deck.yawDeg = yaw;
    addInstance(deck);

    const float deckTop = deckY + deckHalfY;
    for (int end = 0; end <= 1; ++end) {
        const glm::vec3 ep(end == 0 ? ax : bx, deckY, end == 0 ? az : bz);
        for (int side = -1; side <= 1; side += 2) {
            BuildingInstance pier;
            pier.kind = BuildingKind::BridgePier;
            pier.center = ep + perp * (static_cast<float>(side) * kGrid * 1.35f);
            const float ground = terrain.sampleHeight(pier.center.x, pier.center.z);
            const float pierH = deckTop - ground;
            if (pierH < 0.6f)
                continue;
            pier.halfExtents = glm::vec3(1.05f, pierH * 0.5f, 1.05f);
            pier.center.y = ground + pierH * 0.5f;
            addInstance(pier);
        }
    }

    const int railSegs = std::max(2, static_cast<int>(len / (kGrid * 5.0f)));
    const glm::vec3 railStep = ab3 / static_cast<float>(railSegs);
    const float railLen = len / static_cast<float>(railSegs) * 0.9f;
    for (int i = 0; i <= railSegs; ++i) {
        const glm::vec3 pt(ax + railStep.x * static_cast<float>(i), deckY, az + railStep.z * static_cast<float>(i));
        for (int side = -1; side <= 1; side += 2) {
            BuildingInstance rail;
            rail.kind = BuildingKind::BridgeRail;
            rail.center = pt + perp * (static_cast<float>(side) * kGrid * 0.5f);
            rail.center.y = deckTop + 0.2f;
            rail.halfExtents = glm::vec3(railLen * 0.5f, 0.28f, 0.14f);
            rail.yawDeg = yaw;
            addInstance(rail);
        }
    }
}

void MeshBuildingWorld::spawnManor(const TerrainMesh& terrain, int gx, int gz) {
    const int w = 4;
    const int d = 3;
    for (int dz = 0; dz < d; ++dz) {
        for (int dx = 0; dx < w; ++dx) {
            place(terrain, gx + dx, gz + dz, 0, BuildingKind::Foundation);
        }
    }
    for (int layer = 1; layer <= 3; ++layer) {
        for (int dz = 0; dz < d; ++dz) {
            for (int dx = 0; dx < w; ++dx) {
                const bool edge = dx == 0 || dx == w - 1 || dz == 0 || dz == d - 1;
                if (!edge)
                    continue;
                const bool door = dx == w / 2 && dz == 0 && layer == 1;
                place(terrain, gx + dx, gz + dz, layer, door ? BuildingKind::WallWindow : BuildingKind::ManorWall);
            }
        }
    }
    for (int dz = 0; dz < d; ++dz) {
        for (int dx = 0; dx < w; ++dx) {
            BuildingInstance roof = makeInstance(terrain, gx + dx, gz + dz, 4, BuildingKind::ManorRoof);
            roof.halfExtents = glm::vec3(kGrid * 0.52f, 0.5f, kGrid * 0.52f);
            addInstance(roof);
        }
    }
    const int corners[4][2] = {{0, 0}, {w - 1, 0}, {0, d - 1}, {w - 1, d - 1}};
    for (int i = 0; i < 4; ++i) {
        const int cx = gx + corners[i][0];
        const int cz = gz + corners[i][1];
        const float wx = (static_cast<float>(cx) + 0.5f) * kGrid;
        const float wz = (static_cast<float>(cz) + 0.5f) * kGrid;
        const float ground = terrain.sampleHeight(wx, wz);
        const float towerH = kFloorH * 2.8f;

        BuildingInstance tower;
        tower.kind = BuildingKind::ManorTower;
        tower.halfExtents = glm::vec3(1.15f, towerH * 0.5f, 1.15f);
        tower.center = glm::vec3(wx, ground + towerH * 0.5f, wz);
        addInstance(tower);

        BuildingInstance cap;
        cap.kind = BuildingKind::ManorRoof;
        cap.halfExtents = glm::vec3(1.25f, 0.55f, 1.25f);
        cap.center = glm::vec3(wx, ground + towerH + 0.55f, wz);
        addInstance(cap);
    }
}

} // namespace meshcraft
