#include "engine/lighting/LightingSystem.h"

#include "voxel/VoxelWorld.h"

#include <algorithm>
#include <vector>

namespace engine::lighting {

void LightingSystem::buildPointLightsFromWorld(const voxel::VoxelWorld& world, const glm::vec3& cam,
                                               float lampSearchRadius) {
    gpuPoints_.clear();

    struct Cand {
        float d2;
        PointLightGpu gpu;
    };
    std::vector<Cand> cands;
    cands.reserve(64);

    static thread_local std::vector<glm::vec4> lampPacked;
    int lampPairs = 0;
    world.gatherNearbyLamps(cam, kMaxPointLightsGpu, lampSearchRadius, lampPacked, lampPairs);
    for (int i = 0; i < lampPairs; ++i) {
        Cand c;
        c.gpu.posRadius = lampPacked[static_cast<size_t>(i) * 2];
        c.gpu.colIntensity = lampPacked[static_cast<size_t>(i) * 2 + 1];
        const glm::vec3 lp(c.gpu.posRadius.x, c.gpu.posRadius.y, c.gpu.posRadius.z);
        c.d2 = glm::dot(lp - cam, lp - cam);
        cands.push_back(c);
    }

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

} // namespace engine::lighting
