#pragma once

#include <glm/glm.hpp>

namespace engine::physics {

struct Aabb {
    glm::vec3 min{};
    glm::vec3 max{};
    bool overlaps(const Aabb& o) const;
    glm::vec3 center() const;
    glm::vec3 extent() const;
};

/** 体素块 AABB 与实体 AABB 是否相交 */
bool aabbOverlapsBlock(const glm::vec3& pmin, const glm::vec3& pmax, int bx, int by, int bz);

} // namespace engine::physics
