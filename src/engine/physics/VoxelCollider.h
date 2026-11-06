#pragma once

#include "voxel/BlockTypes.h"
#include "voxel/VoxelWorld.h"

#include <glm/glm.hpp>

namespace engine::physics {

/** 将体素世界查询封装为物理碰撞接口 */
class VoxelCollider {
public:
    explicit VoxelCollider(const voxel::VoxelWorld& world);

    bool blocksMovement(voxel::BlockId b) const;
    bool collidesFeetAabb(const glm::vec3& feet, float halfW, float bodyH) const;
    float groundTopAt(float x, float z, float searchFromY) const;
    bool sampleInWater(const glm::vec3& feet, float bodyMidY) const;

private:
    const voxel::VoxelWorld& world_;
};

} // namespace engine::physics
