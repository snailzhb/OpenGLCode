#include "rendering/SceneryRenderer.h"

#include <glad/glad.h>

#include <vector>

void SceneryRenderer::init() {
    shutdown();
    /* 模型空间：X 沿旗面宽度（离杆越远 bend 越大），Y 向上，Z 为法线朝外；每顶点 9 流：pos, normal, uv, bend */
    const int nx = 10;
    const int ny = 8;
    std::vector<float> verts;
    verts.reserve(static_cast<size_t>(nx * ny * 9u));
    for (int j = 0; j < ny; ++j) {
        const float tj = static_cast<float>(j) / static_cast<float>(ny - 1);
        for (int i = 0; i < nx; ++i) {
            const float ti = static_cast<float>(i) / static_cast<float>(nx - 1);
            const float bend = ti * ti;
            const float x = ti * 0.92f;
            const float y = tj * 1.12f;
            const float z = 0.0f;
            verts.push_back(x);
            verts.push_back(y);
            verts.push_back(z);
            verts.push_back(0.0f);
            verts.push_back(0.0f);
            verts.push_back(1.0f);
            verts.push_back(ti);
            verts.push_back(tj);
            verts.push_back(bend);
        }
    }
    std::vector<unsigned int> idx;
    idx.reserve(static_cast<size_t>((nx - 1) * (ny - 1) * 6u));
    auto vid = [nx](int i, int j) -> unsigned int { return static_cast<unsigned int>(j * nx + i); };
    for (int j = 0; j < ny - 1; ++j) {
        for (int i = 0; i < nx - 1; ++i) {
            const unsigned a = vid(i, j);
            const unsigned b = vid(i + 1, j);
            const unsigned c = vid(i + 1, j + 1);
            const unsigned d = vid(i, j + 1);
            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(c);
            idx.push_back(a);
            idx.push_back(c);
            idx.push_back(d);
        }
    }
    indexCount_ = static_cast<int>(idx.size());

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned int)), idx.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(8 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void SceneryRenderer::shutdown() {
    if (ebo_) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    indexCount_ = 0;
}

void SceneryRenderer::drawElements() const {
    if (vao_ == 0 || indexCount_ <= 0)
        return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
