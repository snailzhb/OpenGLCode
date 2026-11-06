#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace engine::lighting {

enum class LightKind : uint8_t { Directional, Point, Spot };

struct DirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity = 1.0f;
    float radius = 12.0f;
    bool castsShadow = false;
};

struct AmbientSettings {
    glm::vec3 skyZenith{0.22f, 0.52f, 0.98f};
    glm::vec3 skyHorizon{0.62f, 0.80f, 1.0f};
    float dayLight = 1.0f;
    float hemisphereStrength = 0.42f;
    float fogDensity = 0.0f;
};

/** GPU 打包：每盏点光 2×vec4(pos+radius, rgb+intensity) */
struct PointLightGpu {
    glm::vec4 posRadius{};
    glm::vec4 colIntensity{};
};

inline constexpr int kMaxPointLightsGpu = 24;

} // namespace engine::lighting
