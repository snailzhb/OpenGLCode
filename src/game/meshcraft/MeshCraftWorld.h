#pragma once

#include "game/meshcraft/MeshBuildingWorld.h"
#include "game/meshcraft/MeshCraftTypes.h"
#include "game/meshcraft/TerrainMesh.h"

#include <glm/glm.hpp>
#include <cstdint>

namespace meshcraft {

struct WorldPickResult {
    bool hit = false;
    bool hitBuilding = false;
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
};

/** 地形 + 建筑 + 场景生成与交互的统一入口 */
class MeshCraftWorld {
public:
    bool build(int terrainResolution = TerrainMesh::kDefaultResolution, uint32_t seed = 2026);
    void shutdownGpu() { terrain_.shutdownGpu(); }

    const TerrainMesh& terrain() const { return terrain_; }
    const MeshBuildingWorld& buildings() const { return buildings_; }
    MeshBuildingWorld& buildings() { return buildings_; }

    glm::vec3 spawnEyePosition() const;
    glm::ivec2 hamletGrid() const { return hamletGrid_; }

    WorldPickResult pick(const glm::vec3& eye, const glm::vec3& dir, float maxDist = 10.0f) const;
    bool tryPlace(BuildingKind kind, const glm::vec3& eye, const glm::vec3& dir, float maxDist = 10.0f);
    bool tryRemove(const glm::vec3& eye, const glm::vec3& dir, float maxDist = 10.0f);

private:
    void seedScene(uint32_t seed);
    glm::ivec2 findHamletSite() const;
    void spawnRiverBridges(uint32_t seed);

    TerrainMesh terrain_;
    MeshBuildingWorld buildings_;
    glm::ivec2 hamletGrid_{0, 0};
};

} // namespace meshcraft
