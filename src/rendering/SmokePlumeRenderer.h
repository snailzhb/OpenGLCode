#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <vector>

namespace voxel {
struct ScenerySmokePuff;
}

/** 实例化 billboard 炊烟：单 draw call，GPU 侧面向相机 */
class SmokePlumeRenderer {
public:
    void init();
    void shutdown();

    bool valid() const { return vao_ != 0; }

    void draw(const glm::mat4& vp, const glm::vec3& camForward, float timeSec,
              const std::vector<voxel::ScenerySmokePuff>& puffs) const;

private:
    unsigned int vao_ = 0;
    unsigned int quadVbo_ = 0;
    unsigned int instanceVbo_ = 0;
    mutable size_t instanceCap_ = 0;

    unsigned int shader_ = 0;
};
