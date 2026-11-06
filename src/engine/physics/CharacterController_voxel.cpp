#include "engine/physics/CharacterController.h"

#include "engine/physics/VoxelCollider.h"

namespace engine::physics {

void CharacterController::tick(VoxelCollider& collider, float dt, const CharacterInput& in) {
    tickCollider(collider, dt, in);
}

} // namespace engine::physics
