#pragma once

#include "engine/Engine.h"
#include "engine/physics/CharacterController.h"
#include "engine/physics/VoxelCollider.h"
#include "voxel/VoxelWorld.h"

#include <functional>
#include <string>

namespace game {

/**
 * 游戏层统一 API：脚本、玩法、工具链通过此门面访问引擎与世界。
 * 由 GameSession 在启动时注册为当前上下文。
 */
class GameAPI {
public:
    static GameAPI& get();

    bool isBound() const;

    void bind(engine::Engine* engine, voxel::VoxelWorld* world, engine::physics::CharacterController* player,
              engine::physics::VoxelCollider* collider);

    engine::Engine& engine();
    voxel::VoxelWorld& world();
    engine::physics::CharacterController& player();
    engine::physics::VoxelCollider& collider();
    engine::lighting::LightingSystem& lighting();
    engine::physics::PhysicsWorld& physics();

    float time() const;
    float deltaTime() const;

    void setToastHandler(std::function<void(const std::string&, float)> handler);
    void toast(const std::string& msg, float durationSec = 3.5f) const;

    void streamAroundPlayer(int halfExtentChunks);
    void placeBlock(int wx, int wy, int wz, voxel::BlockId id);
    voxel::BlockId blockAt(int wx, int wy, int wz) const;

private:
    GameAPI() = default;
    engine::Engine* engine_ = nullptr;
    voxel::VoxelWorld* world_ = nullptr;
    engine::physics::CharacterController* player_ = nullptr;
    engine::physics::VoxelCollider* collider_ = nullptr;
    std::function<void(const std::string&, float)> toast_;
};

} // namespace game
