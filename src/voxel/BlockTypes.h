#pragma once

#include <cstdint>

namespace voxel {

enum class BlockId : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Water,
    Wood,
    Cobble,
    Glass,
    GlassLight,
    GlassDark,
    LampWhite,
    LampWarm,
    LampCyan,
    LampMagenta,
    /** 村落 / 建筑用材质（图集 tile 15–19） */
    Brick,
    HayThatch,
    PathStone,
    MossBrick,
    RoofTerracotta,
    /** 扩展建筑材质（图集 tile 21–28） */
    Slate,
    Plank,
    Marble,
    Iron,
    MossStone,
    FlowerGrass,
    Shingle,
    DarkWood,
    Count
};

inline bool isGlassFamily(BlockId b) {
    return b == BlockId::Glass || b == BlockId::GlassLight || b == BlockId::GlassDark;
}

inline bool isLamp(BlockId b) {
    return b == BlockId::LampWhite || b == BlockId::LampWarm || b == BlockId::LampCyan || b == BlockId::LampMagenta;
}

inline bool isOpaque(BlockId b) {
    return b != BlockId::Air && b != BlockId::Water && !isGlassFamily(b);
}

inline bool isTransparent(BlockId b) {
    return b == BlockId::Water || isGlassFamily(b);
}

inline bool isSolid(BlockId b) {
    return b != BlockId::Air && b != BlockId::Water;
}

/** 右键放置时可替换的空气/水/任意玻璃 */
inline bool isReplaceableWhenPlacing(BlockId b) {
    return b == BlockId::Air || b == BlockId::Water || isGlassFamily(b);
}

} // namespace voxel
