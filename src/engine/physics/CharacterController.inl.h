#pragma once

#include <algorithm>
#include <glm/gtc/constants.hpp>

namespace engine::physics {

template <typename Collider>
void CharacterController::tickCollider(Collider& collider, float dt, const CharacterInput& in) {
    dt = std::min(dt, 0.05f);

    if (in.flyMode) {
        glm::vec3 move = in.wishDirWorld;
        if (glm::dot(move, move) > 1e-6f)
            move = glm::normalize(move);
        feetPosition_ += move * flySpeed_ * dt;
        feetPosition_.y += (in.flyUp - in.flyDown) * flySpeed_ * dt;
        onGround_ = false;
        inWater_ = false;
        velY_ = 0.0f;
        return;
    }

    glm::vec3 horiz(0.0f);
    if (glm::dot(in.wishDirWorld, in.wishDirWorld) > 1e-6f)
        horiz = glm::normalize(glm::vec3(in.wishDirWorld.x, 0.0f, in.wishDirWorld.z));

    inWater_ = collider.sampleInWater(feetPosition_, 0.55f);
    const float speed = inWater_ ? waterSpeed_ : ((in.sprint && onGround_) ? sprintSpeed_ : walkSpeed_);

    if (inWater_) {
        velY_ *= 1.0f - glm::clamp(dt * 3.5f, 0.0f, 0.35f);
        velY_ += 5.5f * dt;
        if (in.jump)
            velY_ += 10.0f * dt;
    } else {
        velY_ -= gravity_ * dt;
        if (in.jump && onGround_) {
            velY_ = jumpSpeed_;
            onGround_ = false;
        }
    }

    auto tryHoriz = [&](float dx, float dz) {
        const glm::vec3 trial = feetPosition_ + glm::vec3(dx, 0.0f, dz);
        if (!collider.collidesFeetAabb(trial, halfWidth_, bodyHeight_))
            feetPosition_ = trial;
    };
    tryHoriz(horiz.x * speed * dt, horiz.z * speed * dt);

    onGround_ = false;
    {
        const glm::vec3 trial = feetPosition_ + glm::vec3(0.0f, velY_ * dt, 0.0f);
        if (!collider.collidesFeetAabb(trial, halfWidth_, bodyHeight_)) {
            feetPosition_ = trial;
        } else if (velY_ < 0.0f) {
            onGround_ = true;
            velY_ = 0.0f;
            const float gy = collider.groundTopAt(feetPosition_.x, feetPosition_.z, feetPosition_.y + 0.05f);
            if (feetPosition_.y < gy + 0.02f)
                feetPosition_.y = gy;
        } else {
            velY_ = 0.0f;
        }
    }

    if (onGround_ && !inWater_) {
        const float gy = collider.groundTopAt(feetPosition_.x, feetPosition_.z, feetPosition_.y + 0.2f);
        if (feetPosition_.y < gy + 0.08f)
            feetPosition_.y = gy;
        velY_ = 0.0f;
    }
}

} // namespace engine::physics
