#pragma once

#include "engine/Subsystem.h"
#include "engine/lighting/LightTypes.h"

#include <learnopengl/shader.h>

#include <glm/glm.hpp>
#include <vector>

namespace voxel {
class VoxelWorld;
}

namespace engine::lighting {

/** 方向光 + 环境 + 点光（路灯/动态光源）统一管理与着色器上传 */
class LightingSystem : public Subsystem {
public:
    const char* name() const override { return "Lighting"; }

    void tick(float dt) override;
    void clearDynamicPointLights();

    void setDirectional(const DirectionalLight& sun);
    void setAmbient(const AmbientSettings& amb);
    const DirectionalLight& directional() const { return sun_; }
    const AmbientSettings& ambient() const { return ambient_; }

    void addPointLight(const PointLight& light);
    void addFlickerTorch(const glm::vec3& pos, const glm::vec3& color, float baseIntensity, float radius,
                         float phase);

    /** 从体素世界收集路灯并合并动态点光，按距离排序截断 */
    void buildPointLightsFromWorld(const voxel::VoxelWorld& world, const glm::vec3& cam, float lampSearchRadius);

    /** 仅使用 addPointLight / addFlickerTorch 注册的动态光（MeshCraft 等无体素路灯场景） */
    void buildDynamicPointLights(const glm::vec3& cam);

    void uploadToShader(Shader& shader) const;

    const std::vector<PointLightGpu>& gpuPointLights() const { return gpuPoints_; }

private:
    DirectionalLight sun_{};
    AmbientSettings ambient_{};
    std::vector<PointLight> dynamicPoints_;
    std::vector<PointLightGpu> gpuPoints_;
    float timeSec_ = 0.0f;
};

} // namespace engine::lighting
