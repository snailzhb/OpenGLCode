#include "rendering/ScaledBlockRenderer.h"

#include "engine/lighting/LightingSystem.h"
#include "voxel/VoxelWorld.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#ifndef SHADER_ROOT
#define SHADER_ROOT "shaders"
#endif

namespace {

void pushFace(std::vector<float>& v, const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3,
              const glm::vec3& n, float u0, float v0, float u1, float v1) {
    const float tri[6][8] = {
        {p0.x, p0.y, p0.z, n.x, n.y, n.z, u0, v0}, {p2.x, p2.y, p2.z, n.x, n.y, n.z, u1, v1},
        {p1.x, p1.y, p1.z, n.x, n.y, n.z, u1, v0}, {p0.x, p0.y, p0.z, n.x, n.y, n.z, u0, v0},
        {p3.x, p3.y, p3.z, n.x, n.y, n.z, u0, v1}, {p2.x, p2.y, p2.z, n.x, n.y, n.z, u1, v1},
    };
    for (int i = 0; i < 6; ++i)
        for (int j = 0; j < 8; ++j)
            v.push_back(tri[i][j]);
}

void buildUnitCubeMesh(std::vector<float>& out) {
    out.clear();
    const float h = 0.5f;
    const glm::vec3 px0(h, -h, -h), px1(h, -h, h), px2(h, h, h), px3(h, h, -h);
    const glm::vec3 nx0(-h, -h, h), nx1(-h, -h, -h), nx2(-h, h, -h), nx3(-h, h, h);
    const glm::vec3 py0(-h, h, -h), py1(-h, h, h), py2(h, h, h), py3(h, h, -h);
    const glm::vec3 ny0(-h, -h, -h), ny1(-h, -h, h), ny2(h, -h, h), ny3(h, -h, -h);
    const glm::vec3 pz0(h, -h, h), pz1(-h, -h, h), pz2(-h, h, h), pz3(h, h, h);
    const glm::vec3 nz0(-h, -h, -h), nz1(h, -h, -h), nz2(h, h, -h), nz3(-h, h, -h);
    pushFace(out, px0, px1, px2, px3, glm::vec3(1, 0, 0), 0, 0, 1, 1);
    pushFace(out, nx0, nx1, nx2, nx3, glm::vec3(-1, 0, 0), 0, 0, 1, 1);
    pushFace(out, py0, py1, py2, py3, glm::vec3(0, 1, 0), 0, 0, 1, 1);
    pushFace(out, ny0, ny1, ny2, ny3, glm::vec3(0, -1, 0), 0, 0, 1, 1);
    pushFace(out, pz0, pz1, pz2, pz3, glm::vec3(0, 0, 1), 0, 0, 1, 1);
    pushFace(out, nz0, nz1, nz2, nz3, glm::vec3(0, 0, -1), 0, 0, 1, 1);
}

} // namespace

void ScaledBlockRenderer::init() {
    shutdown();
    const std::string sd = std::string(SHADER_ROOT) + "/";
    try {
        shader_ = std::make_unique<Shader>((sd + "scaled_voxel.vs").c_str(), (sd + "scaled_voxel.fs").c_str());
    } catch (...) {
        std::cerr << "ScaledBlockRenderer: shader load failed\n";
        shader_.reset();
        return;
    }

    std::vector<float> mesh;
    buildUnitCubeMesh(mesh);
    meshVertexCount_ = static_cast<int>(mesh.size() / 8);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &meshVbo_);
    glGenBuffers(1, &instanceVbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.size() * sizeof(float)), mesh.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, static_cast<int>(8 * sizeof(float)), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, static_cast<int>(8 * sizeof(float)), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, static_cast<int>(8 * sizeof(float)), reinterpret_cast<void*>(6 * sizeof(float)));

    instanceCap_ = 512;
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * instanceCap_, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, static_cast<int>(sizeof(glm::vec4) * 2), reinterpret_cast<void*>(0));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, static_cast<int>(sizeof(glm::vec4) * 2), reinterpret_cast<void*>(sizeof(glm::vec4)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ScaledBlockRenderer::shutdown() {
    if (meshVbo_) {
        glDeleteBuffers(1, &meshVbo_);
        meshVbo_ = 0;
    }
    if (instanceVbo_) {
        glDeleteBuffers(1, &instanceVbo_);
        instanceVbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    shader_.reset();
    meshVertexCount_ = 0;
    instanceCap_ = 0;
}

void ScaledBlockRenderer::applyLighting(const engine::lighting::LightingSystem& lighting) const {
    if (shader_)
        lighting.uploadToShader(*shader_);
}

void ScaledBlockRenderer::draw(const glm::mat4& vp, const glm::mat4& lightSpace, const glm::vec3& sunDir,
                               const glm::vec3& viewPos, const glm::vec3& skyZenith, const glm::vec3& skyHorizon,
                               float dayLight, unsigned int atlasTex, unsigned int shadowTex, float shadowInvSize,
                               const std::vector<voxel::SceneryScaledVoxel>& instances) const {
    if (!valid() || instances.empty())
        return;

    const size_t n = instances.size();
    if (n > instanceCap_) {
        instanceCap_ = std::max(instanceCap_ * 2, n);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * instanceCap_, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    std::vector<glm::vec4> interleaved;
    interleaved.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        interleaved[i * 2] = instances[i].posScale;
        interleaved[i * 2 + 1] = instances[i].tilePad;
    }
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(interleaved.size() * sizeof(glm::vec4)), interleaved.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    shader_->use();
    shader_->setMat4("uMvp", vp);
    shader_->setMat4("uLightSpace", lightSpace);
    shader_->setVec3("uSunDir", sunDir);
    shader_->setVec3("uViewPos", viewPos);
    shader_->setVec3("uSkyZenith", skyZenith);
    shader_->setVec3("uSkyHorizon", skyHorizon);
    shader_->setFloat("uDayLight", dayLight);
    shader_->setVec2("uShadowTexelSize", glm::vec2(shadowInvSize, shadowInvSize));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTex);
    shader_->setInt("uAtlas", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    shader_->setInt("uShadowMap", 1);

    glBindVertexArray(vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, meshVertexCount_, static_cast<GLsizei>(n));
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}
