#pragma once

#include "engine/physics/Collision.h"

#include <glm/glm.hpp>

namespace engine::physics {

enum class BodyType : uint8_t { Static, Kinematic, Dynamic };

struct RigidBody {
    BodyType type = BodyType::Dynamic;
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 halfExtents{0.5f};
    float mass = 1.0f;
    float restitution = 0.0f;
    float friction = 0.85f;
    bool useGravity = true;
    bool onGround = false;
    int userData = 0;

    Aabb aabb() const;
};

struct CharacterInput {
    glm::vec3 wishDirWorld{0.0f};
    bool jump = false;
    bool sprint = false;
    bool flyMode = false;
    float flyUp = 0.0f;
    float flyDown = 0.0f;
};

} // namespace engine::physics
