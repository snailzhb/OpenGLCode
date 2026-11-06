#include "engine/Engine.h"

namespace engine {

bool Engine::init() {
    subsystems_.clear();
    subsystems_.push_back(&physics_);
    subsystems_.push_back(&lighting_);
    for (Subsystem* s : subsystems_)
        s->onAttach();
    return true;
}

void Engine::shutdown() {
    for (auto it = subsystems_.rbegin(); it != subsystems_.rend(); ++it)
        (*it)->onDetach();
    subsystems_.clear();
}

void Engine::beginFrame(float dt) {
    dt_ = dt;
    totalTime_ += dt;
    for (Subsystem* s : subsystems_)
        s->tick(dt);
}

void Engine::endFrame() {}

} // namespace engine
