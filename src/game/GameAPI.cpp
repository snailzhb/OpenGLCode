#include "game/GameAPI.h"

namespace game {

namespace {
engine::Engine& fallbackEngine() {
    static engine::Engine e;
    return e;
}
} // namespace

GameAPI& GameAPI::get() {
    static GameAPI api;
    return api;
}

void GameAPI::bind(engine::Engine* eng, voxel::VoxelWorld* world, engine::physics::CharacterController* player,
                   engine::physics::VoxelCollider* collider) {
    engine_ = eng;
    world_ = world;
    player_ = player;
    collider_ = collider;
}

bool GameAPI::isBound() const {
    return engine_ != nullptr && world_ != nullptr && player_ != nullptr && collider_ != nullptr;
}

engine::Engine& GameAPI::engine() {
    return engine_ ? *engine_ : fallbackEngine();
}

voxel::VoxelWorld& GameAPI::world() {
    return *world_;
}

engine::physics::CharacterController& GameAPI::player() {
    return *player_;
}

engine::physics::VoxelCollider& GameAPI::collider() {
    return *collider_;
}

engine::lighting::LightingSystem& GameAPI::lighting() {
    return engine().lighting();
}

engine::physics::PhysicsWorld& GameAPI::physics() {
    return engine().physics();
}

float GameAPI::time() const {
    return engine_ ? engine_->totalTime() : 0.0f;
}

float GameAPI::deltaTime() const {
    return engine_ ? engine_->deltaTime() : 0.0f;
}

void GameAPI::setToastHandler(std::function<void(const std::string&, float)> handler) {
    toast_ = std::move(handler);
}

void GameAPI::toast(const std::string& msg, float durationSec) const {
    if (toast_)
        toast_(msg, durationSec);
}

void GameAPI::streamAroundPlayer(int halfExtentChunks) {
    if (!isBound())
        return;
    const glm::vec3 eye = player_->eyePosition();
    world_->streamAround(static_cast<int>(std::floor(eye.x)), static_cast<int>(std::floor(eye.z)), halfExtentChunks);
}

void GameAPI::placeBlock(int wx, int wy, int wz, voxel::BlockId id) {
    if (world_)
        world_->setBlock(wx, wy, wz, id);
}

voxel::BlockId GameAPI::blockAt(int wx, int wy, int wz) const {
    return world_ ? world_->getBlock(wx, wy, wz) : voxel::BlockId::Air;
}

} // namespace game
