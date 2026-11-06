#pragma once

#include "engine/lighting/LightingSystem.h"
#include "engine/physics/PhysicsWorld.h"

#include <memory>
#include <vector>

namespace engine {

/** GameEigen 运行时：子系统调度、帧计时 */
class Engine {
public:
    bool init();
    void shutdown();

    void beginFrame(float dt);
    void endFrame();

    float deltaTime() const { return dt_; }
    float totalTime() const { return totalTime_; }

    physics::PhysicsWorld& physics() { return physics_; }
    lighting::LightingSystem& lighting() { return lighting_; }
    const physics::PhysicsWorld& physics() const { return physics_; }
    const lighting::LightingSystem& lighting() const { return lighting_; }

private:
    float dt_ = 0.0f;
    float totalTime_ = 0.0f;
    physics::PhysicsWorld physics_;
    lighting::LightingSystem lighting_;
    std::vector<Subsystem*> subsystems_;
};

} // namespace engine
