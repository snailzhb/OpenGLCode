#pragma once

/** 体素块 RGBA 图集：优先加载 resources/textures/voxel_atlas.png，失败则程序生成；法线图为 voxel_atlas_n.png 或由漫反射生成 */
struct VoxelAtlas {
    unsigned texture = 0;
    unsigned normalTexture = 0;

    bool init();
    void shutdown();

    static constexpr int kTiles = 28;
    static constexpr int kTilePx = 128;
    static constexpr int kAtlasW = kTiles * kTilePx;
    static constexpr int kAtlasH = kTilePx;
};
