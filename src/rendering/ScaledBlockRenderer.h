#pragma once

#include <learnopengl/shader.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace engine::lighting {
class LightingSystem;
}
namespace voxel {
struct SceneryScaledVoxel;
}

/** GPU 实例化：不同 uniform scale 的体素装饰块（与主 chunk 网格独立） */
class ScaledBlockRenderer {
public:
    void init();
    void shutdown();

    bool valid() const { return vao_ != 0 && shader_; }

    void applyLighting(const engine::lighting::LightingSystem& lighting) const;

    void draw(const glm::mat4& vp, const glm::mat4& lightSpace, const glm::vec3& sunDir, const glm::vec3& viewPos,
              const glm::vec3& skyZenith, const glm::vec3& skyHorizon, float dayLight, unsigned int atlasTex,
              unsigned int shadowTex, float shadowInvSize, const std::vector<voxel::SceneryScaledVoxel>& instances) const;

private:
    unsigned int vao_ = 0;
    unsigned int meshVbo_ = 0;
    mutable unsigned int instanceVbo_ = 0;
    mutable size_t instanceCap_ = 0;
    int meshVertexCount_ = 0;
    std::unique_ptr<Shader> shader_;
};
