#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

namespace meshcraft {

/** Y 轴旋转的 OBB：建筑实例射线与碰撞 */
struct OrientedBox {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtents{1.0f};
    float yawDeg = 0.0f;

    glm::mat3 rotationY() const {
        const float r = glm::radians(yawDeg);
        const float c = std::cos(r);
        const float s = std::sin(r);
        return glm::mat3(c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c);
    }

    glm::vec3 worldToLocal(const glm::vec3& w) const {
        return glm::transpose(rotationY()) * (w - center);
    }

    glm::vec3 localToWorld(const glm::vec3& l) const {
        return center + rotationY() * l;
    }

    bool containsXZ(const glm::vec3& world, float pad = 0.0f) const {
        const glm::vec3 l = worldToLocal(world);
        return std::abs(l.x) <= halfExtents.x + pad && std::abs(l.z) <= halfExtents.z + pad;
    }

    bool raycast(const glm::vec3& origin, const glm::vec3& dir, float tMax, float& outT, glm::vec3& outNormal) const {
        const glm::mat3 R = rotationY();
        const glm::vec3 oL = glm::transpose(R) * (origin - center);
        const glm::vec3 dL = glm::transpose(R) * dir;

        float tNear = 0.0f;
        float tFar = tMax;
        glm::vec3 nL(0.0f);

        for (int axis = 0; axis < 3; ++axis) {
            const float o = oL[axis];
            const float d = dL[axis];
            const float h = halfExtents[axis];
            if (std::abs(d) < 1e-8f) {
                if (o < -h || o > h)
                    return false;
                continue;
            }
            float t0 = (-h - o) / d;
            float t1 = (h - o) / d;
            glm::vec3 n0(0.0f);
            glm::vec3 n1(0.0f);
            n0[axis] = d > 0.0f ? -1.0f : 1.0f;
            n1[axis] = d > 0.0f ? 1.0f : -1.0f;
            if (t0 > t1) {
                std::swap(t0, t1);
                std::swap(n0, n1);
            }
            if (t0 > tNear) {
                tNear = t0;
                nL = n0;
            }
            if (t1 < tFar)
                tFar = t1;
            if (tNear > tFar || tFar < 0.0f)
                return false;
        }
        if (tNear < 0.0f || tNear > tMax)
            return false;
        outT = tNear;
        outNormal = glm::normalize(R * nL);
        return true;
    }

    glm::vec3 worldAabbMin() const {
        const glm::mat3 R = rotationY();
        glm::vec3 ex(0.0f);
        for (int i = 0; i < 3; ++i) {
            ex.x += std::abs(R[0][i]) * halfExtents[i];
            ex.y += std::abs(R[1][i]) * halfExtents[i];
            ex.z += std::abs(R[2][i]) * halfExtents[i];
        }
        return center - ex;
    }

    glm::vec3 worldAabbMax() const {
        const glm::mat3 R = rotationY();
        glm::vec3 ex(0.0f);
        for (int i = 0; i < 3; ++i) {
            ex.x += std::abs(R[0][i]) * halfExtents[i];
            ex.y += std::abs(R[1][i]) * halfExtents[i];
            ex.z += std::abs(R[2][i]) * halfExtents[i];
        }
        return center + ex;
    }
};

inline OrientedBox instanceBox(const glm::vec3& center, const glm::vec3& halfExtents, float yawDeg) {
    OrientedBox b;
    b.center = center;
    b.halfExtents = halfExtents;
    b.yawDeg = yawDeg;
    return b;
}

} // namespace meshcraft
