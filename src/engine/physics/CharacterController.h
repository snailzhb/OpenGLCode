#pragma once

#include "engine/physics/PhysicsTypes.h"
#include "engine/physics/VoxelCollider.h"

#include <glm/glm.hpp>

namespace engine::physics {

class MeshTerrainCollider;

/** 第一人称角色：行走 / 跳跃 / 水中 / 飞行 */
class CharacterController {
public:
    static constexpr float kDefaultEyeHeight = 1.62f;
    static constexpr float kDefaultHalfW = 0.28f;
    static constexpr float kDefaultBodyH = 1.75f;

    glm::vec3 eyePosition() const { return feetPosition_ + glm::vec3(0.0f, eyeHeight_, 0.0f); }
    glm::vec3 feetPosition() const { return feetPosition_; }
    void setEyePosition(const glm::vec3& eye) { feetPosition_ = eye - glm::vec3(0.0f, eyeHeight_, 0.0f); }

    bool onGround() const { return onGround_; }
    bool inWater() const { return inWater_; }
    float verticalVelocity() const { return velY_; }

    void tick(VoxelCollider& collider, float dt, const CharacterInput& in);
    void tick(MeshTerrainCollider& collider, float dt, const CharacterInput& in);

    float eyeHeight_ = kDefaultEyeHeight;
    float halfWidth_ = kDefaultHalfW;
    float bodyHeight_ = kDefaultBodyH;
    float walkSpeed_ = 7.2f;
    float sprintSpeed_ = 13.5f;
    float waterSpeed_ = 4.5f;
    float flySpeed_ = 9.0f;
    float jumpSpeed_ = 7.8f;
    float gravity_ = 26.0f;

private:
    template <typename Collider>
    void tickCollider(Collider& collider, float dt, const CharacterInput& in);

    glm::vec3 feetPosition_{0.0f};
    float velY_ = 0.0f;
    bool onGround_ = false;
    bool inWater_ = false;
};

} // namespace engine::physics

#include "engine/physics/CharacterController.inl.h"
