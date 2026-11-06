#pragma once

#include "game/meshcraft/MeshCraftTypes.h"
#include "game/meshcraft/TerrainMesh.h"

#include <glm/glm.hpp>
#include <vector>

namespace meshcraft {

struct BuildingInstance {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{1.0f, 1.25f, 1.0f};
    BuildingKind kind = BuildingKind::Foundation;
    float yawDeg = 0.0f;
};

/** 网格对齐的建筑放置与碰撞 */
class MeshBuildingWorld {
public:
    static constexpr float kGrid = 2.0f;
    static constexpr float kFloorH = 2.5f;

    void snapRayHitToCell(const TerrainMesh& terrain, const glm::vec3& hit, const glm::vec3& normal, glm::ivec3& outCell,
                          int& outLayer) const;

    bool canPlace(const TerrainMesh& terrain, int gx, int gz, int layer, BuildingKind kind) const;
    bool place(const TerrainMesh& terrain, int gx, int gz, int layer, BuildingKind kind);
    bool removeAt(const glm::vec3& worldPos, float radius = 1.6f);

    /** maxDist 入参为最大距离；命中时写回实际距离 */
    bool raycastSolid(const glm::vec3& origin, const glm::vec3& dir, float& maxDist, glm::vec3& hit,
                      glm::vec3* outNormal = nullptr, glm::ivec3* outCell = nullptr) const;

    bool collidesAabb(const glm::vec3& pmin, const glm::vec3& pmax) const;

    const std::vector<BuildingInstance>& instances() const { return instances_; }
    void clear() { instances_.clear(); }

    void seedStarterHamlet(const TerrainMesh& terrain, int centerGx, int centerGz);
    void spawnBridge(const TerrainMesh& terrain, float ax, float az, float bx, float bz);
    void spawnManor(const TerrainMesh& terrain, int gx, int gz);

private:
    void addInstance(const BuildingInstance& bi);
    glm::vec3 cellCenterWorld(const TerrainMesh& terrain, int gx, int gz, int layer) const;
    BuildingInstance makeInstance(const TerrainMesh& terrain, int gx, int gz, int layer, BuildingKind kind) const;
    int findAt(int gx, int gz, int layer) const;

    std::vector<BuildingInstance> instances_;
};

} // namespace meshcraft
