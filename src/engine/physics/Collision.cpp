#include "engine/physics/Collision.h"

namespace engine::physics {

bool Aabb::overlaps(const Aabb& o) const {
    return min.x < o.max.x && max.x > o.min.x && min.y < o.max.y && max.y > o.min.y && min.z < o.max.z &&
           max.z > o.min.z;
}

glm::vec3 Aabb::center() const {
    return (min + max) * 0.5f;
}

glm::vec3 Aabb::extent() const {
    return max - min;
}

bool aabbOverlapsBlock(const glm::vec3& pmin, const glm::vec3& pmax, int bx, int by, int bz) {
    const glm::vec3 bmin(static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bz));
    const glm::vec3 bmax = bmin + glm::vec3(1.0f);
    return pmin.x < bmax.x && pmax.x > bmin.x && pmin.y < bmax.y && pmax.y > bmin.y && pmin.z < bmax.z &&
           pmax.z > bmin.z;
}

} // namespace engine::physics
