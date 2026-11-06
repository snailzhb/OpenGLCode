#include "rendering/MeshSceneRenderer.h"

#include "engine/lighting/LightingSystem.h"
#include "game/meshcraft/MeshCraftTypes.h"

#include <glad/glad.h>

#include <iostream>
#include <string>
#include <vector>

#ifndef SHADER_ROOT
#define SHADER_ROOT "shaders"
#endif

namespace {

bool programLinked(unsigned int program) {
    if (program == 0)
        return false;
    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    return ok == GL_TRUE;
}

} // namespace

void MeshSceneRenderer::init() {
    shutdown();
    const std::string sd = std::string(SHADER_ROOT) + "/";
    try {
        meshShader_ = std::make_unique<Shader>((sd + "mesh_lit.vs").c_str(), (sd + "mesh_lit.fs").c_str());
        depthShader_ = std::make_unique<Shader>((sd + "mesh_depth.vs").c_str(), (sd + "mesh_depth.fs").c_str());
        waterShader_ = std::make_unique<Shader>((sd + "mesh_water.vs").c_str(), (sd + "mesh_water.fs").c_str());
    } catch (...) {
        std::cerr << "MeshSceneRenderer: shader load failed\n";
        meshShader_.reset();
        depthShader_.reset();
        waterShader_.reset();
        return;
    }
    if (meshShader_ && !programLinked(meshShader_->ID)) {
        std::cerr << "MeshSceneRenderer: mesh_lit shader failed to link\n";
        meshShader_.reset();
    }
    if (depthShader_ && !programLinked(depthShader_->ID)) {
        std::cerr << "MeshSceneRenderer: mesh_depth shader failed to link\n";
        depthShader_.reset();
    }
    if (waterShader_ && !programLinked(waterShader_->ID)) {
        std::cerr << "MeshSceneRenderer: mesh_water shader failed to link\n";
        waterShader_.reset();
    }
    if (!atlas_.init()) {
        std::cerr << "MeshSceneRenderer: atlas init failed\n";
        meshShader_.reset();
        depthShader_.reset();
        waterShader_.reset();
        return;
    }
    rigid_.initUnitCube();
    setupInstancedAttributes();
}

void MeshSceneRenderer::setupInstancedAttributes() {
    if (instAttribsReady_ || rigid_.unitCubeVao() == 0 || rigid_.unitCubeVbo() == 0)
        return;

    glGenVertexArrays(1, &instVao_);
    glGenBuffers(1, &instVbo_);

    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, rigid_.unitCubeVbo());
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 3 * 64, nullptr, GL_DYNAMIC_DRAW);
    const GLsizei stride = static_cast<GLsizei>(3 * sizeof(glm::vec4));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(glm::vec4)));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(glm::vec4)));
    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    instAttribsReady_ = true;
    instCap_ = 64;
}

void MeshSceneRenderer::shutdown() {
    if (instVbo_) {
        glDeleteBuffers(1, &instVbo_);
        instVbo_ = 0;
    }
    if (instVao_) {
        glDeleteVertexArrays(1, &instVao_);
        instVao_ = 0;
    }
    instAttribsReady_ = false;
    instCap_ = 0;
    depthShader_.reset();
    waterShader_.reset();
    meshShader_.reset();
    atlas_.shutdown();
    rigid_.shutdown();
}

namespace {

void bindMeshTextures(unsigned int atlasTex, unsigned int shadowTex, Shader& shader) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    shader.setInt("uAtlas", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTex);
    shader.setInt("uShadowMap", 1);
}

} // namespace

void MeshSceneRenderer::ensureInstanceCapacity(size_t n) const {
    if (!instAttribsReady_)
        return;
    if (n <= instCap_)
        return;
    instCap_ = std::max(n, instCap_ * 2 + 64);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 3 * instCap_, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshSceneRenderer::drawTerrain(const meshcraft::TerrainMesh& terrain, const glm::mat4& vp,
                                    const glm::mat4& lightSpace, const glm::vec3& viewPos, unsigned int shadowTex,
                                    float shadowInvSize, const engine::lighting::LightingSystem& lighting) const {
    if (!meshShader_)
        return;
    meshShader_->use();
    lighting.uploadToShader(*meshShader_);
    meshShader_->setMat4("uMvp", vp);
    meshShader_->setMat4("uModel", glm::mat4(1.0f));
    meshShader_->setMat4("uLightSpace", lightSpace);
    meshShader_->setVec3("uViewPos", viewPos);
    meshShader_->setInt("uInstanced", 0);
    meshShader_->setInt("uTerrainMode", 1);
    bindMeshTextures(atlas_.texture, shadowTex, *meshShader_);
    meshShader_->setVec2("uShadowTexelSize", glm::vec2(shadowInvSize, shadowInvSize));
    terrain.draw();
}

void MeshSceneRenderer::drawBuildings(const meshcraft::MeshBuildingWorld& world, const glm::mat4& vp,
                                      const glm::mat4& lightSpace, const glm::vec3& viewPos, unsigned int shadowTex,
                                      float shadowInvSize, const engine::lighting::LightingSystem& lighting) const {
    if (!meshShader_ || !instAttribsReady_ || world.instances().empty())
        return;
    const auto& list = world.instances();
    ensureInstanceCapacity(list.size());
    std::vector<glm::vec4> packed;
    packed.resize(list.size() * 3);
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& b = list[i];
        packed[i * 3 + 0] = glm::vec4(b.center, 0.0f);
        packed[i * 3 + 1] = glm::vec4(b.halfExtents, 0.0f);
        packed[i * 3 + 2] = glm::vec4(static_cast<float>(meshcraft::materialId(b.kind)), b.yawDeg, 0.0f, 0.0f);
    }
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(packed.size() * sizeof(glm::vec4)), packed.data());

    meshShader_->use();
    lighting.uploadToShader(*meshShader_);
    meshShader_->setMat4("uMvp", vp);
    meshShader_->setMat4("uLightSpace", lightSpace);
    meshShader_->setVec3("uViewPos", viewPos);
    meshShader_->setInt("uInstanced", 1);
    meshShader_->setInt("uTerrainMode", 0);
    bindMeshTextures(atlas_.texture, shadowTex, *meshShader_);
    meshShader_->setVec2("uShadowTexelSize", glm::vec2(shadowInvSize, shadowInvSize));
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, static_cast<GLsizei>(list.size()));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshSceneRenderer::drawWater(const meshcraft::TerrainMesh& terrain, const glm::mat4& vp, float timeSec,
                                  const glm::vec3& viewPos, const glm::vec3& sunDir, const glm::vec3& sunColor,
                                  const glm::vec3& skyHorizon, const glm::vec3& skyZenith, float dayLight) const {
    if (!waterShader_)
        return;
    waterShader_->use();
    waterShader_->setMat4("uMvp", vp);
    waterShader_->setFloat("uTime", timeSec);
    waterShader_->setVec3("uViewPos", viewPos);
    waterShader_->setVec3("uSunDir", sunDir);
    waterShader_->setVec3("uSunColor", sunColor);
    waterShader_->setVec3("uSkyHorizon", skyHorizon);
    waterShader_->setVec3("uSkyZenith", skyZenith);
    waterShader_->setFloat("uDayLight", dayLight);
    terrain.drawWater();
}

void MeshSceneRenderer::drawShadowTerrain(const meshcraft::TerrainMesh& terrain, const glm::mat4& lightMvp) const {
    if (!depthShader_)
        return;
    depthShader_->use();
    depthShader_->setMat4("uLightMvp", lightMvp);
    depthShader_->setMat4("uModel", glm::mat4(1.0f));
    depthShader_->setInt("uInstanced", 0);
    terrain.draw();
}

void MeshSceneRenderer::drawShadowBuildings(const meshcraft::MeshBuildingWorld& world, const glm::mat4& lightMvp) const {
    if (!depthShader_ || !instAttribsReady_ || world.instances().empty())
        return;
    const auto& list = world.instances();
    std::vector<glm::vec4> packed;
    packed.reserve(list.size() * 3);
    for (const auto& b : list) {
        if (b.kind == meshcraft::BuildingKind::BridgeRail)
            continue;
        packed.push_back(glm::vec4(b.center, 0.0f));
        packed.push_back(glm::vec4(b.halfExtents, 0.0f));
        packed.push_back(glm::vec4(static_cast<float>(meshcraft::materialId(b.kind)), b.yawDeg, 0.0f, 0.0f));
    }
    if (packed.empty())
        return;
    const size_t nInst = packed.size() / 3;
    ensureInstanceCapacity(nInst);
    glBindVertexArray(instVao_);
    glBindBuffer(GL_ARRAY_BUFFER, instVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(packed.size() * sizeof(glm::vec4)), packed.data());
    depthShader_->use();
    depthShader_->setMat4("uLightMvp", lightMvp);
    depthShader_->setInt("uInstanced", 1);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, static_cast<GLsizei>(nInst));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
