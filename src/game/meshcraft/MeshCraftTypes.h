#pragma once

#include <cstdint>

namespace meshcraft {

enum class BuildingKind : uint8_t {
    Foundation = 0,
    Wall,
    WallWindow,
    Pillar,
    Roof,
    Stairs,
    BridgeDeck,
    BridgePier,
    BridgeRail,
    ManorWall,
    ManorRoof,
    ManorTower,
    Count
};

inline int materialId(BuildingKind k) {
    switch (k) {
    case BuildingKind::Foundation:
        return 11;
    case BuildingKind::Wall:
        return 10;
    case BuildingKind::WallWindow:
        return 13;
    case BuildingKind::Pillar:
        return 11;
    case BuildingKind::Roof:
        return 12;
    case BuildingKind::Stairs:
        return 10;
    case BuildingKind::BridgeDeck:
        return 6;
    case BuildingKind::BridgePier:
        return 3;
    case BuildingKind::BridgeRail:
        return 10;
    case BuildingKind::ManorWall:
        return 10;
    case BuildingKind::ManorRoof:
        return 12;
    case BuildingKind::ManorTower:
        return 3;
    default:
        return 10;
    }
}

} // namespace meshcraft
