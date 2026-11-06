#pragma once

#include "engine/Subsystem.h"
#include "engine/physics/PhysicsTypes.h"
#include "engine/physics/VoxelCollider.h"

#include <vector>

namespace engine::physics {

/** 体素 AABB 物理：角色控制器 + 可选动态刚体 */
class PhysicsWorld : public Subsystem {
public:
    const char* name() const override { return "Physics"; }

    void setCollider(VoxelCollider* collider);
    void tick(float dt) override;

    int addBody(const RigidBody& body);
    RigidBody* body(int id);
    const std::vector<RigidBody>& bodies() const { return bodies_; }

    void setGravity(float g) { gravity_ = g; }
    float gravity() const { return gravity_; }

private:
    VoxelCollider* collider_ = nullptr;
    std::vector<RigidBody> bodies_;
    float gravity_ = 26.0f;
};

} // namespace engine::physics
