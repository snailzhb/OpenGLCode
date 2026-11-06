#pragma once

#include <glm/glm.hpp>

namespace meshcraft {
class MeshBuildingWorld;
class TerrainMesh;
}

namespace engine::physics {

/** 高度场 + 建筑 AABB 碰撞（MeshCraft 专用） */
class MeshTerrainCollider {
public:
    MeshTerrainCollider(const meshcraft::TerrainMesh& terrain, const meshcraft::MeshBuildingWorld& buildings);

    bool collidesFeetAabb(const glm::vec3& feet, float halfW, float bodyH) const;
    float groundTopAt(float x, float z, float searchFromY) const;
    bool sampleInWater(const glm::vec3& feet, float bodyMidY) const;

private:
    const meshcraft::TerrainMesh& terrain_;
    const meshcraft::MeshBuildingWorld& buildings_;
};

} // namespace engine::physics
