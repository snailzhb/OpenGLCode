#include "rendering/RigidMesh.h"

#include <cmath>
#include <vector>

void RigidMesh::initUnitCube() {
    float vertices[] = {
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f,
        0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
    glGenVertexArrays(1, &cubeVao_);
    glGenBuffers(1, &cubeVbo_);
    glBindVertexArray(cubeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    glGenVertexArrays(1, &lampVao_);
    glBindVertexArray(lampVao_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void RigidMesh::initUnitQuad() {
    if (quadVao_)
        return;
    /* XY 平面，法线 +Z，面向 +Z 方向（billboard 第三列为视线） */
    float q[] = {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
               -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    glGenVertexArrays(1, &quadVao_);
    glGenBuffers(1, &quadVbo_);
    glBindVertexArray(quadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void RigidMesh::initUnitSphere() {
    if (sphereVao_)
        return;
    const int latBands = 12;
    const int lonSegments = 16;
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(static_cast<size_t>((latBands + 1) * (lonSegments + 1)) * 8u);
    for (int lat = 0; lat <= latBands; ++lat) {
        const float v = static_cast<float>(lat) / static_cast<float>(latBands);
        const float theta = v * 3.14159265f;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (int lon = 0; lon <= lonSegments; ++lon) {
            const float u = static_cast<float>(lon) / static_cast<float>(lonSegments);
            const float phi = u * 2.0f * 3.14159265f;
            const float x = std::cos(phi) * sinTheta;
            const float y = cosTheta;
            const float z = std::sin(phi) * sinTheta;
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
            vertices.push_back(u);
            vertices.push_back(v);
        }
    }
    for (int lat = 0; lat < latBands; ++lat) {
        for (int lon = 0; lon < lonSegments; ++lon) {
            const unsigned int first = static_cast<unsigned int>(lat * (lonSegments + 1) + lon);
            const unsigned int second = first + static_cast<unsigned int>(lonSegments + 1);
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1u);
            indices.push_back(second);
            indices.push_back(second + 1u);
            indices.push_back(first + 1u);
        }
    }
    sphereIndexCount_ = static_cast<int>(indices.size());
    glGenVertexArrays(1, &sphereVao_);
    glGenBuffers(1, &sphereVbo_);
    glGenBuffers(1, &sphereEbo_);
    glBindVertexArray(sphereVao_);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void RigidMesh::shutdown() {
    if (sphereEbo_)
        glDeleteBuffers(1, &sphereEbo_);
    if (sphereVbo_)
        glDeleteBuffers(1, &sphereVbo_);
    if (sphereVao_)
        glDeleteVertexArrays(1, &sphereVao_);
    if (lampVao_)
        glDeleteVertexArrays(1, &lampVao_);
    if (cubeVao_)
        glDeleteVertexArrays(1, &cubeVao_);
    if (cubeVbo_)
        glDeleteBuffers(1, &cubeVbo_);
    if (quadVao_)
        glDeleteVertexArrays(1, &quadVao_);
    if (quadVbo_)
        glDeleteBuffers(1, &quadVbo_);
    sphereEbo_ = sphereVbo_ = sphereVao_ = 0;
    sphereIndexCount_ = 0;
    lampVao_ = cubeVao_ = cubeVbo_ = quadVao_ = quadVbo_ = 0;
}

void RigidMesh::drawCube() const {
    glBindVertexArray(cubeVao_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void RigidMesh::drawLampCube() const {
    glBindVertexArray(lampVao_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void RigidMesh::drawQuad() const {
    if (!quadVao_)
        return;
    glBindVertexArray(quadVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void RigidMesh::drawSphere() const {
    if (!sphereVao_ || sphereIndexCount_ <= 0)
        return;
    glBindVertexArray(sphereVao_);
    glDrawElements(GL_TRIANGLES, sphereIndexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
