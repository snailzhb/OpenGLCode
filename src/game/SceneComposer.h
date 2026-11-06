#pragma once

#include "engine/lighting/LightingSystem.h"
#include "voxel/VoxelWorld.h"

#include <cstdint>

namespace game {

/** 在村落生成之后追加大型场景：市场、码头、城墙、环境光 */
class SceneComposer {
public:
    static void enrichHubRegion(voxel::VoxelWorld& world, engine::lighting::LightingSystem& lighting, int anchorWx,
                               int anchorWz, uint32_t seed);
    static void enrichRegionalCluster(voxel::VoxelWorld& world, engine::lighting::LightingSystem& lighting, int anchorWx,
                                      int anchorWz, uint32_t seed);
};

} // namespace game
