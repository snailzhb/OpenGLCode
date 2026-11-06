#pragma once

#include "game/meshcraft/MeshBuildingWorld.h"
#include "game/meshcraft/TerrainMesh.h"
#include "rendering/RigidMesh.h"
#include "rendering/VoxelAtlas.h"

#include <learnopengl/shader.h>

#include <glm/glm.hpp>
#include <memory>

namespace engine::lighting {
class LightingSystem;
}

/** 绘制 MeshCraft 地形 + 实例化建筑 */
class MeshSceneRenderer {
public:
    void init();
    void shutdown();
    bool valid() const { return meshShader_ != nullptr && atlas_.texture != 0; }

    void drawTerrain(const meshcraft::TerrainMesh& terrain, const glm::mat4& vp, const glm::mat4& lightSpace,
                     const glm::vec3& viewPos, unsigned int shadowTex, float shadowInvSize,
                     const engine::lighting::LightingSystem& lighting) const;
    void drawBuildings(const meshcraft::MeshBuildingWorld& world, const glm::mat4& vp, const glm::mat4& lightSpace,
                       const glm::vec3& viewPos, unsigned int shadowTex, float shadowInvSize,
                       const engine::lighting::LightingSystem& lighting) const;
    void drawShadowTerrain(const meshcraft::TerrainMesh& terrain, const glm::mat4& lightMvp) const;
    void drawShadowBuildings(const meshcraft::MeshBuildingWorld& world, const glm::mat4& lightMvp) const;
    void drawWater(const meshcraft::TerrainMesh& terrain, const glm::mat4& vp, float timeSec, const glm::vec3& viewPos,
                   const glm::vec3& sunDir, const glm::vec3& sunColor, const glm::vec3& skyHorizon,
                   const glm::vec3& skyZenith, float dayLight) const;

    RigidMesh& rigidMesh() { return rigid_; }

private:
    void setupInstancedAttributes();
    void ensureInstanceCapacity(size_t n) const;

    RigidMesh rigid_;
    VoxelAtlas atlas_;
    std::unique_ptr<Shader> meshShader_;
    std::unique_ptr<Shader> depthShader_;
    std::unique_ptr<Shader> waterShader_;
    bool instAttribsReady_ = false;
    unsigned int instVao_ = 0;
    mutable unsigned int instVbo_ = 0;
    mutable size_t instCap_ = 0;
};
