#include "engine/physics/PhysicsWorld.h"

#include <algorithm>

namespace engine::physics {

void PhysicsWorld::tick(float dt) {
    if (!collider_)
        return;
    dt = std::min(dt, 0.05f);

    for (auto& b : bodies_) {
        if (b.type != BodyType::Dynamic)
            continue;
        if (b.useGravity)
            b.velocity.y -= gravity_ * dt;

        glm::vec3 trial = b.position + b.velocity * dt;
        const glm::vec3 feet = trial - glm::vec3(0.0f, b.halfExtents.y, 0.0f);
        const float hw = b.halfExtents.x;
        const float bh = b.halfExtents.y * 2.0f;

        b.onGround = false;
        if (collider_->collidesFeetAabb(feet, hw, bh)) {
            if (b.velocity.y < 0.0f) {
                b.velocity.y = 0.0f;
                const float gy = collider_->groundTopAt(feet.x, feet.z, feet.y + 0.1f);
                trial.y = gy + b.halfExtents.y;
                b.onGround = true;
            } else {
                b.velocity.y = 0.0f;
            }
        }
        b.position = trial;
        b.velocity *= (1.0f - b.friction * dt);
    }
}

} // namespace engine::physics
