#include "game/SceneComposer.h"

#include "voxel/BlockTypes.h"

#include <cmath>

namespace game {

namespace {

int terrainY(const voxel::VoxelWorld& world, int wx, int wz) {
    return voxel::VoxelWorld::terrainHeight(wx, wz);
}

void stampPlatform(voxel::VoxelWorld& world, int cx, int cz, int half, int yTop, voxel::BlockId surface) {
    for (int dx = -half; dx <= half; ++dx) {
        for (int dz = -half; dz <= half; ++dz) {
            const int wx = cx + dx;
            const int wz = cz + dz;
            const int th = terrainY(world, wx, wz);
            for (int y = th + 1; y < yTop; ++y)
                world.setBlock(wx, y, wz, voxel::BlockId::Cobble);
            world.setBlock(wx, yTop, wz, surface);
        }
    }
}

} // namespace

void SceneComposer::enrichHubRegion(voxel::VoxelWorld& world, engine::lighting::LightingSystem& lighting, int anchorWx,
                                    int anchorWz, uint32_t seed) {
    const float ax = static_cast<float>(anchorWx);
    const float az = static_cast<float>(anchorWz);
    constexpr float avoid = 26.0f;

    /* —— 东门市场 —— */
    const int marketX = anchorWx + 38;
    const int marketZ = anchorWz;
    stampPlatform(world, marketX, marketZ, 14, terrainY(world, marketX, marketZ) + 1, voxel::BlockId::PathStone);
    for (int stall = 0; stall < 8; ++stall) {
        const float ang = static_cast<float>(stall) * 6.2831853f / 8.0f;
        const float sx = static_cast<float>(marketX) + std::cos(ang) * 8.0f;
        const float sz = static_cast<float>(marketZ) + std::sin(ang) * 8.0f;
        world.addScaledProp(sx, sz, 1.2f + static_cast<float>(stall % 3) * 0.35f, static_cast<int>(voxel::BlockId::Plank),
                            ang * 57.2957795f, anchorWx, anchorWz, avoid);
        world.addScaledProp(sx + 1.2f, sz, 0.65f, static_cast<int>(voxel::BlockId::HayThatch), 0.0f, anchorWx,
                            anchorWz, avoid);
        lighting.addFlickerTorch(glm::vec3(sx, static_cast<float>(terrainY(world, static_cast<int>(sx), static_cast<int>(sz))) + 2.2f,
                                 sz),
                                 glm::vec3(1.0f, 0.72f, 0.38f), 1.4f, 9.0f, static_cast<float>(stall) * 0.7f);
    }
    for (int i = 0; i < 4; ++i) {
        world.addScaledProp(static_cast<float>(marketX) + static_cast<float>(i * 5 - 8),
                            static_cast<float>(marketZ) + 10.0f, 2.4f, static_cast<int>(voxel::BlockId::DarkWood), 0.0f,
                            anchorWx, anchorWz, avoid);
    }

    /* —— 南码头 —— */
    const int dockX = anchorWx;
    const int dockZ = anchorWz + 42;
    for (int wx = dockX - 12; wx <= dockX + 12; ++wx) {
        for (int wz = dockZ - 4; wz <= dockZ + 14; ++wz) {
            const int sy = terrainY(world, wx, wz);
            world.setBlock(wx, sy, wz, voxel::BlockId::PathStone);
            if (wz > dockZ + 6)
                world.setBlock(wx, sy, wz, voxel::BlockId::Water);
        }
    }
    for (int p = -10; p <= 10; ++p) {
        const int wx = dockX + p;
        const int sy = terrainY(world, wx, dockZ);
        world.setBlock(wx, sy + 1, dockZ, voxel::BlockId::Wood);
        world.setBlock(wx, sy + 2, dockZ, voxel::BlockId::Wood);
        if (p % 4 == 0)
            world.setBlock(wx, sy + 3, dockZ, voxel::BlockId::LampWarm);
    }
    world.addScaledProp(static_cast<float>(dockX) + 6.0f, static_cast<float>(dockZ) + 8.0f, 3.2f,
                        static_cast<int>(voxel::BlockId::DarkWood), 15.0f, anchorWx, anchorWz, avoid);
    world.addScaledProp(static_cast<float>(dockX) - 7.0f, static_cast<float>(dockZ) + 9.0f, 2.1f,
                        static_cast<int>(voxel::BlockId::Iron), 0.0f, anchorWx, anchorWz, avoid);
    lighting.addPointLight({glm::vec3(static_cast<float>(dockX), static_cast<float>(terrainY(world, dockX, dockZ)) + 4.0f,
                          static_cast<float>(dockZ) + 2.0f),
                          glm::vec3(0.45f, 0.85f, 1.0f), 1.8f, 22.0f});

    /* —— 外圈城墙（简化环形） —— */
    const int wallR = 58;
    const int step = 4;
    for (int deg = 0; deg < 360; deg += step) {
        const float rad = static_cast<float>(deg) * 0.017453292f;
        const int wx = anchorWx + static_cast<int>(std::cos(rad) * static_cast<float>(wallR));
        const int wz = anchorWz + static_cast<int>(std::sin(rad) * static_cast<float>(wallR));
        const int sy = terrainY(world, wx, wz);
        for (int h = 1; h <= 4; ++h)
            world.setBlock(wx, sy + h, wz, (h == 4) ? voxel::BlockId::MossStone : voxel::BlockId::Cobble);
        if (deg % 24 == 0) {
            world.setBlock(wx, sy + 5, wz, voxel::BlockId::LampCyan);
            lighting.addFlickerTorch(
                glm::vec3(static_cast<float>(wx) + 0.5f, static_cast<float>(sy + 5), static_cast<float>(wz) + 0.5f),
                glm::vec3(0.4f, 0.9f, 1.0f), 1.2f, 14.0f, static_cast<float>(deg));
        }
    }

    /* —— 西北仓储区 —— */
    const int whX = anchorWx - 44;
    const int whZ = anchorWz - 36;
    for (int bx = 0; bx < 3; ++bx) {
        for (int bz = 0; bz < 2; ++bz) {
            const int ox = whX + bx * 10;
            const int oz = whZ + bz * 12;
            stampPlatform(world, ox + 4, oz + 5, 4, terrainY(world, ox, oz) + 1, voxel::BlockId::Plank);
            world.addScaledProp(static_cast<float>(ox) + 4.5f, static_cast<float>(oz) + 5.5f, 3.8f,
                                static_cast<int>(voxel::BlockId::Plank), static_cast<float>(bx * 40 + bz * 17), anchorWx,
                                anchorWz, avoid);
            world.addScaledProp(static_cast<float>(ox) + 2.0f, static_cast<float>(oz) + 3.0f, 1.6f,
                                static_cast<int>(voxel::BlockId::HayThatch), 0.0f, anchorWx, anchorWz, avoid);
        }
    }

    /* —— 环境点光：广场篝火 —— */
    lighting.addFlickerTorch(glm::vec3(ax + 0.5f, static_cast<float>(terrainY(world, anchorWx, anchorWz)) + 2.5f, az + 0.5f),
                             glm::vec3(1.0f, 0.55f, 0.22f), 2.2f, 16.0f, 0.0f);
    lighting.addFlickerTorch(glm::vec3(ax - 8.0f, static_cast<float>(terrainY(world, anchorWx - 8, anchorWz)) + 2.0f,
                             az + 6.0f),
                             glm::vec3(1.0f, 0.48f, 0.18f), 1.6f, 12.0f, 1.1f);
    (void)seed;
}

void SceneComposer::enrichRegionalCluster(voxel::VoxelWorld& world, engine::lighting::LightingSystem& lighting,
                                          int anchorWx, int anchorWz, uint32_t seed) {
    constexpr float avoid = 16.0f;
    const float ang = static_cast<float>(seed % 360u) * 0.017453292f;
    const int fx = anchorWx + static_cast<int>(std::cos(ang) * 14.0f);
    const int fz = anchorWz + static_cast<int>(std::sin(ang) * 14.0f);
    stampPlatform(world, fx, fz, 5, terrainY(world, fx, fz) + 1, voxel::BlockId::Sand);
    world.addScaledProp(static_cast<float>(fx), static_cast<float>(fz), 1.8f, static_cast<int>(voxel::BlockId::Stone), 0.0f,
                        anchorWx, anchorWz, avoid);
    lighting.addFlickerTorch(
        glm::vec3(static_cast<float>(fx) + 0.5f, static_cast<float>(terrainY(world, fx, fz)) + 2.0f,
                  static_cast<float>(fz) + 0.5f),
        glm::vec3(1.0f, 0.7f, 0.35f), 1.1f, 10.0f, static_cast<float>(seed));
}

} // namespace game
