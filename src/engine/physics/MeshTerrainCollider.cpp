#include "engine/physics/MeshTerrainCollider.h"

#include "game/meshcraft/BuildingGeom.h"
#include "game/meshcraft/MeshBuildingWorld.h"
#include "game/meshcraft/TerrainMesh.h"

#include <algorithm>
#include <cmath>

namespace engine::physics {

MeshTerrainCollider::MeshTerrainCollider(const meshcraft::TerrainMesh& terrain,
                                         const meshcraft::MeshBuildingWorld& buildings)
    : terrain_(terrain), buildings_(buildings) {}

bool MeshTerrainCollider::collidesFeetAabb(const glm::vec3& feet, float halfW, float bodyH) const {
    const glm::vec3 pmin(feet.x - halfW, feet.y + 0.04f, feet.z - halfW);
    const glm::vec3 pmax(feet.x + halfW, feet.y + bodyH, feet.z + halfW);
    if (buildings_.collidesAabb(pmin, pmax))
        return true;

    const int x0 = static_cast<int>(std::floor(pmin.x));
    const int x1 = static_cast<int>(std::floor(pmax.x));
    const int z0 = static_cast<int>(std::floor(pmin.z));
    const int z1 = static_cast<int>(std::floor(pmax.z));
    for (int z = z0; z <= z1; ++z) {
        for (int x = x0; x <= x1; ++x) {
            const float ground = terrain_.sampleHeight(static_cast<float>(x), static_cast<float>(z));
            if (pmin.y < ground + 0.15f)
                return true;
        }
    }
    return false;
}

float MeshTerrainCollider::groundTopAt(float x, float z, float searchFromY) const {
    float top = terrain_.sampleHeight(x, z);
    const glm::vec3 probe(x, searchFromY, z);
    for (const auto& b : buildings_.instances()) {
        const meshcraft::OrientedBox box =
            meshcraft::instanceBox(b.center, b.halfExtents, b.yawDeg);
        if (!box.containsXZ(probe, 0.08f))
            continue;
        top = std::max(top, box.worldAabbMax().y);
    }
    return top;
}

bool MeshTerrainCollider::sampleInWater(const glm::vec3& feet, float bodyMidY) const {
    (void)bodyMidY;
    return terrain_.sampleIsWater(feet.x, feet.z);
}

} // namespace engine::physics
