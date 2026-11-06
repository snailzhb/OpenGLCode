#include "engine/physics/PhysicsWorld.h"

namespace engine::physics {

void PhysicsWorld::tick(float dt) {
    (void)dt;
    /* MeshCraft 不使用体素刚体；collider_ 恒为 nullptr */
}

} // namespace engine::physics
