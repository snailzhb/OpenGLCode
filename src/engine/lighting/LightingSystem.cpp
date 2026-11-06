#include "engine/lighting/LightingSystem.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace engine::lighting {

void LightingSystem::tick(float dt) {
    timeSec_ += dt;
}

void LightingSystem::clearDynamicPointLights() {
    dynamicPoints_.clear();
}

void LightingSystem::setDirectional(const DirectionalLight& sun) {
    sun_ = sun;
}

void LightingSystem::setAmbient(const AmbientSettings& amb) {
    ambient_ = amb;
}

void LightingSystem::addPointLight(const PointLight& light) {
    dynamicPoints_.push_back(light);
}

void LightingSystem::addFlickerTorch(const glm::vec3& pos, const glm::vec3& color, float baseIntensity, float radius,
                                     float phase) {
    const float flick = 0.88f + 0.12f * std::sin(timeSec_ * 5.5f + phase);
    PointLight pl;
    pl.position = pos;
    pl.color = color;
    pl.intensity = baseIntensity * flick;
    pl.radius = radius;
    dynamicPoints_.push_back(pl);
}

void LightingSystem::buildDynamicPointLights(const glm::vec3& cam) {
    gpuPoints_.clear();
    struct Cand {
        float d2;
        PointLightGpu gpu;
    };
    std::vector<Cand> cands;
    cands.reserve(dynamicPoints_.size());
    for (const PointLight& pl : dynamicPoints_) {
        Cand c;
        c.gpu.posRadius = glm::vec4(pl.position, pl.radius);
        c.gpu.colIntensity = glm::vec4(pl.color * pl.intensity, pl.intensity);
        c.d2 = glm::dot(pl.position - cam, pl.position - cam);
        cands.push_back(c);
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.d2 < b.d2; });
    const int cap = std::min(static_cast<int>(cands.size()), kMaxPointLightsGpu);
    for (int i = 0; i < cap; ++i)
        gpuPoints_.push_back(cands[static_cast<size_t>(i)].gpu);
}

void LightingSystem::uploadToShader(Shader& shader) const {
    shader.use();
    shader.setVec3("uSunDir", sun_.direction);
    shader.setVec3("uSunColor", sun_.color * sun_.intensity);
    shader.setVec3("uSkyZenith", ambient_.skyZenith);
    shader.setVec3("uSkyHorizon", ambient_.skyHorizon);
    shader.setFloat("uDayLight", ambient_.dayLight);
    shader.setFloat("uHemiStrength", ambient_.hemisphereStrength);

    const int n = static_cast<int>(gpuPoints_.size());
    shader.setInt("uPointLightCount", n);

    const GLint locBuf = glGetUniformLocation(shader.ID, "uPointLightBuf");
    if (locBuf >= 0) {
        if (n > 0) {
            std::vector<glm::vec4> flat;
            flat.reserve(static_cast<size_t>(n) * 2);
            for (const PointLightGpu& g : gpuPoints_) {
                flat.push_back(g.posRadius);
                flat.push_back(g.colIntensity);
            }
            glUniform4fv(locBuf, n * 2, &flat[0][0]);
        } else {
            static const glm::vec4 zeros[kMaxPointLightsGpu * 2]{};
            glUniform4fv(locBuf, kMaxPointLightsGpu * 2, &zeros[0][0]);
        }
    }

    /* 兼容旧 uniform 名（部分着色器仍读 uLampPairCount） */
    const GLint locLegacy = glGetUniformLocation(shader.ID, "uLampPairCount");
    if (locLegacy >= 0)
        glUniform1i(locLegacy, n);
    const GLint locLegacyBuf = glGetUniformLocation(shader.ID, "uLampBuf");
    if (locLegacyBuf >= 0 && locBuf >= 0) {
        if (n > 0) {
            std::vector<glm::vec4> flat;
            flat.reserve(static_cast<size_t>(n) * 2);
            for (const PointLightGpu& g : gpuPoints_) {
                flat.push_back(g.posRadius);
                flat.push_back(g.colIntensity);
            }
            glUniform4fv(locLegacyBuf, n * 2, &flat[0][0]);
        }
    }
}

} // namespace engine::lighting
