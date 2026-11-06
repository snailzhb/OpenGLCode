#include "engine/physics/PhysicsWorld.h"

#include <algorithm>
#include <cmath>

namespace engine::physics {

Aabb RigidBody::aabb() const {
    Aabb box;
    box.min = position - halfExtents;
    box.max = position + halfExtents;
    return box;
}

void PhysicsWorld::setCollider(VoxelCollider* collider) {
    collider_ = collider;
}

int PhysicsWorld::addBody(const RigidBody& body) {
    bodies_.push_back(body);
    return static_cast<int>(bodies_.size()) - 1;
}

RigidBody* PhysicsWorld::body(int id) {
    if (id < 0 || id >= static_cast<int>(bodies_.size()))
        return nullptr;
    return &bodies_[static_cast<size_t>(id)];
}

} // namespace engine::physics
