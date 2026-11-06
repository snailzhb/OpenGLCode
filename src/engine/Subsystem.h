#pragma once

namespace engine {

/** 引擎子系统：在 Engine::beginFrame / endFrame 间按序 tick */
class Subsystem {
public:
    virtual ~Subsystem() = default;
    virtual const char* name() const = 0;
    virtual void onAttach() {}
    virtual void onDetach() {}
    virtual void tick(float dt) = 0;
};

} // namespace engine
