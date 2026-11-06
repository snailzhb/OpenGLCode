#include "engine/physics/CharacterController.h"

#include "engine/physics/MeshTerrainCollider.h"

namespace engine::physics {

void CharacterController::tick(MeshTerrainCollider& collider, float dt, const CharacterInput& in) {
    tickCollider(collider, dt, in);
}

} // namespace engine::physics
