#include "engine/physics/VoxelCollider.h"

#include "engine/physics/Collision.h"

#include <cmath>

namespace engine::physics {

VoxelCollider::VoxelCollider(const voxel::VoxelWorld& world) : world_(world) {}

bool VoxelCollider::blocksMovement(voxel::BlockId b) const {
    return voxel::isSolid(b) && b != voxel::BlockId::Water;
}

bool VoxelCollider::collidesFeetAabb(const glm::vec3& feet, float halfW, float bodyH) const {
    const glm::vec3 pmin(feet.x - halfW, feet.y + 0.04f, feet.z - halfW);
    const glm::vec3 pmax(feet.x + halfW, feet.y + bodyH, feet.z + halfW);
    const int x0 = static_cast<int>(std::floor(pmin.x));
    const int x1 = static_cast<int>(std::floor(pmax.x));
    const int y0 = static_cast<int>(std::floor(pmin.y));
    const int y1 = static_cast<int>(std::floor(pmax.y));
    const int z0 = static_cast<int>(std::floor(pmin.z));
    const int z1 = static_cast<int>(std::floor(pmax.z));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                if (blocksMovement(world_.getBlock(x, y, z)) && aabbOverlapsBlock(pmin, pmax, x, y, z))
                    return true;
            }
        }
    }
    return false;
}

float VoxelCollider::groundTopAt(float x, float z, float searchFromY) const {
    const int ix = static_cast<int>(std::floor(x));
    const int iz = static_cast<int>(std::floor(z));
    const int yStart = static_cast<int>(std::floor(searchFromY));
    for (int y = yStart; y >= yStart - 12; --y) {
        if (blocksMovement(world_.getBlock(ix, y, iz)))
            return static_cast<float>(y + 1);
    }
    return searchFromY - 12.0f;
}

bool VoxelCollider::sampleInWater(const glm::vec3& feet, float bodyMidY) const {
    const int wx = static_cast<int>(std::floor(feet.x));
    const int wy = static_cast<int>(std::floor(feet.y + bodyMidY));
    const int wz = static_cast<int>(std::floor(feet.z));
    return world_.getBlock(wx, wy, wz) == voxel::BlockId::Water ||
           world_.getBlock(wx, wy + 1, wz) == voxel::BlockId::Water;
}

} // namespace engine::physics
