#include "app/VoxelApplication.h"

#include "engine/lighting/LightTypes.h"
#include "root_directory.h"
#include "voxel/VoxelConstants.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {

/** 与 VoxelWorld::worldToChunk 一致的负坐标除法 */
int floorChunkDiv(int a) {
    constexpr int b = voxel::kChunkSize;
    const int q = a / b;
    const int r = a % b;
    if ((r != 0) && ((r > 0) != (b > 0)))
        return q - 1;
    return q;
}

bool chunkInDrawRange(const voxel::Chunk* ch, const glm::vec3& cam) {
    const float s = static_cast<float>(voxel::kChunkSize);
    const glm::vec3 mn(s * static_cast<float>(ch->cx), s * static_cast<float>(ch->cy), s * static_cast<float>(ch->cz));
    const glm::vec3 center = mn + glm::vec3(s * 0.5f);
    const float ext = 0.8660254f * s;
    if (glm::length(center - cam) > voxel::kChunkDrawMaxDist + ext)
        return false;
    /* 仅用距离做粗剔除：视锥-AABB 在部分视角下会误判，导致整块世界不画只剩天空色 */
    return true;
}

/** 正交阴影体附近：剔除远处 chunk，减轻阴影 pass 的 draw 次数 */
bool chunkNearShadowCastDistance(const voxel::Chunk* ch, const glm::vec3& focus, float orthoExtent) {
    constexpr float kPad = 16.0f;
    const float mx = static_cast<float>(ch->cx * voxel::kChunkSize + voxel::kChunkSize / 2);
    const float my = static_cast<float>(ch->cy * voxel::kChunkSize + voxel::kChunkSize / 2);
    const float mz = static_cast<float>(ch->cz * voxel::kChunkSize + voxel::kChunkSize / 2);
    if (std::abs(mx - focus.x) > orthoExtent + kPad)
        return false;
    if (std::abs(mz - focus.z) > orthoExtent + kPad)
        return false;
    if (std::abs(my - focus.y) > orthoExtent * 1.35f + 42.0f)
        return false;
    return true;
}

std::string shaderDir() {
#ifdef SHADER_ROOT
    return std::string(SHADER_ROOT) + "/";
#else
    return std::string("shaders/");
#endif
}

const char* blockNameEn(voxel::BlockId b) {
    switch (b) {
    case voxel::BlockId::Grass:
        return "Grass";
    case voxel::BlockId::Dirt:
        return "Dirt";
    case voxel::BlockId::Stone:
        return "Stone";
    case voxel::BlockId::Sand:
        return "Sand";
    case voxel::BlockId::Wood:
        return "Wood";
    case voxel::BlockId::Cobble:
        return "Cobble";
    case voxel::BlockId::Glass:
        return "Glass";
    case voxel::BlockId::GlassLight:
        return "GlassLt";
    case voxel::BlockId::GlassDark:
        return "GlassDk";
    case voxel::BlockId::LampWhite:
        return "LampW";
    case voxel::BlockId::LampWarm:
        return "LampWarm";
    case voxel::BlockId::LampCyan:
        return "LampCyan";
    case voxel::BlockId::LampMagenta:
        return "LampMag";
    case voxel::BlockId::Brick:
        return "Brick";
    case voxel::BlockId::PathStone:
        return "Path";
    case voxel::BlockId::HayThatch:
        return "Thatch";
    default:
        return "?";
    }
}

glm::vec3 blockHudColor(voxel::BlockId b) {
    switch (b) {
    case voxel::BlockId::Grass:
        return glm::vec3(0.35f, 0.72f, 0.28f);
    case voxel::BlockId::Dirt:
        return glm::vec3(0.55f, 0.38f, 0.22f);
    case voxel::BlockId::Stone:
        return glm::vec3(0.55f, 0.55f, 0.58f);
    case voxel::BlockId::Sand:
        return glm::vec3(0.86f, 0.78f, 0.48f);
    case voxel::BlockId::Wood:
        return glm::vec3(0.58f, 0.40f, 0.22f);
    case voxel::BlockId::Cobble:
        return glm::vec3(0.48f, 0.50f, 0.52f);
    case voxel::BlockId::Glass:
        return glm::vec3(0.62f, 0.82f, 0.92f);
    case voxel::BlockId::GlassLight:
        return glm::vec3(0.88f, 0.94f, 1.0f);
    case voxel::BlockId::GlassDark:
        return glm::vec3(0.28f, 0.38f, 0.52f);
    case voxel::BlockId::LampWhite:
        return glm::vec3(1.0f, 0.98f, 0.92f);
    case voxel::BlockId::LampWarm:
        return glm::vec3(1.0f, 0.72f, 0.38f);
    case voxel::BlockId::LampCyan:
        return glm::vec3(0.45f, 0.92f, 1.0f);
    case voxel::BlockId::LampMagenta:
        return glm::vec3(1.0f, 0.45f, 0.88f);
    default:
        return glm::vec3(0.7f);
    }
}

constexpr float kEyeHeight = 1.62f;
constexpr float kPlayerHalfW = 0.28f;
constexpr float kPlayerBodyH = 1.75f;

bool blocksPlayer(voxel::BlockId b) {
    return voxel::isSolid(b) && b != voxel::BlockId::Water;
}

bool aabbOverlapBlock(const glm::vec3& pmin, const glm::vec3& pmax, int bx, int by, int bz) {
    const glm::vec3 bmin(static_cast<float>(bx), static_cast<float>(by), static_cast<float>(bz));
    const glm::vec3 bmax = bmin + glm::vec3(1.0f);
    return pmin.x < bmax.x && pmax.x > bmin.x && pmin.y < bmax.y && pmax.y > bmin.y && pmin.z < bmax.z &&
           pmax.z > bmin.z;
}

bool playerCollidesAt(const voxel::VoxelWorld& world, const glm::vec3& feet) {
    const glm::vec3 pmin(feet.x - kPlayerHalfW, feet.y + 0.04f, feet.z - kPlayerHalfW);
    const glm::vec3 pmax(feet.x + kPlayerHalfW, feet.y + kPlayerBodyH, feet.z + kPlayerHalfW);
    const int x0 = static_cast<int>(std::floor(pmin.x));
    const int x1 = static_cast<int>(std::floor(pmax.x));
    const int y0 = static_cast<int>(std::floor(pmin.y));
    const int y1 = static_cast<int>(std::floor(pmax.y));
    const int z0 = static_cast<int>(std::floor(pmin.z));
    const int z1 = static_cast<int>(std::floor(pmax.z));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            for (int z = z0; z <= z1; ++z) {
                if (blocksPlayer(world.getBlock(x, y, z)) && aabbOverlapBlock(pmin, pmax, x, y, z))
                    return true;
            }
        }
    }
    return false;
}

float groundTopAt(const voxel::VoxelWorld& world, float x, float z, float searchFromY) {
    const int ix = static_cast<int>(std::floor(x));
    const int iz = static_cast<int>(std::floor(z));
    const int yStart = static_cast<int>(std::floor(searchFromY));
    for (int y = yStart; y >= yStart - 10; --y) {
        if (blocksPlayer(world.getBlock(ix, y, iz)))
            return static_cast<float>(y + 1);
    }
    return searchFromY - 10.0f;
}

voxel::BlockId cycleLamp(voxel::BlockId b) {
    switch (b) {
    case voxel::BlockId::LampWarm:
        return voxel::BlockId::LampWhite;
    case voxel::BlockId::LampWhite:
        return voxel::BlockId::LampCyan;
    case voxel::BlockId::LampCyan:
        return voxel::BlockId::LampMagenta;
    case voxel::BlockId::LampMagenta:
        return voxel::BlockId::LampWarm;
    default:
        return voxel::BlockId::LampWarm;
    }
}

struct DayNight {
    /** 天空日盘 / 暮光色调用（真实太阳高度，可低于地平线） */
    glm::vec3 visualSunDir{};
    /** 体素/水面/旗帜/阴影用：夜间改为偏高「月光」方向，避免太阳在地平线下时阴影矩阵与 N·L 全失效 */
    glm::vec3 shadeSunDir{};
    glm::vec3 skyHorizon{};
    glm::vec3 skyZenith{};
    float dayLight = 1.0f;
};

DayNight computeDayNight(float timeSec) {
    constexpr float kTwoPi = 6.2831853f;
    constexpr float period = 240.0f;
    const float a = timeSec * (kTwoPi / period);
    const glm::vec3 sunFromVisual =
        glm::normalize(glm::vec3(std::cos(a) * 0.58f, 0.17f + 0.83f * std::sin(a), std::sin(a * 0.74f) * 0.42f));
    const float el = sunFromVisual.y;
    float dayLight = glm::clamp((el + 0.18f) / 1.02f, 0.0f, 1.0f);
    dayLight = glm::max(dayLight, 0.08f);

    const glm::vec3 moonFrom = glm::normalize(glm::vec3(0.48f, 0.62f, 0.36f));
    const float w = glm::smoothstep(-0.18f, 0.22f, el);
    const glm::vec3 sunFromShade = glm::normalize(glm::mix(moonFrom, sunFromVisual, w));

    const glm::vec3 horizonDay(0.62f, 0.80f, 1.0f);
    const glm::vec3 zenithDay(0.22f, 0.52f, 0.98f);
    const glm::vec3 horizonNight(0.04f, 0.05f, 0.12f);
    const glm::vec3 zenithNight(0.005f, 0.015f, 0.06f);
    const float blend = glm::pow(dayLight, 0.82f);
    DayNight d;
    d.visualSunDir = -sunFromVisual;
    d.shadeSunDir = -sunFromShade;
    d.skyHorizon = glm::mix(horizonNight, horizonDay, blend);
    d.skyZenith = glm::mix(zenithNight, zenithDay, blend);
    d.dayLight = dayLight;
    return d;
}

glm::vec3 eulerToForward(float yawDeg, float pitchDeg) {
    const float y = glm::radians(yawDeg);
    const float p = glm::radians(pitchDeg);
    glm::vec3 f;
    f.x = std::cos(y) * std::cos(p);
    f.y = std::sin(p);
    f.z = std::sin(y) * std::cos(p);
    return glm::normalize(f);
}

/** 沿相机竖直方向采样：只有体素为「水」时才视为水下，避免仅用海面高度把陆地误判 */
float eyeInWaterFactor(const voxel::VoxelWorld& world, const glm::vec3& cam) {
    int hits = 0;
    for (int i = 0; i < 6; ++i) {
        const float oy = cam.y - static_cast<float>(i) * 0.22f;
        const int x = static_cast<int>(std::floor(cam.x));
        const int y = static_cast<int>(std::floor(oy));
        const int z = static_cast<int>(std::floor(cam.z));
        if (world.getBlock(x, y, z) == voxel::BlockId::Water)
            ++hits;
    }
    return glm::clamp(static_cast<float>(hits) / 2.5f, 0.0f, 1.0f);
}

void syncLightingFromScene(Shader& sh, engine::lighting::LightingSystem& lighting, const voxel::VoxelWorld& world,
                           const glm::vec3& cam, const DayNight& dn) {
    engine::lighting::DirectionalLight sun;
    sun.direction = dn.shadeSunDir;
    sun.color = glm::vec3(1.14f, 1.02f, 0.86f);
    sun.intensity = glm::mix(0.45f, 1.0f, dn.dayLight);
    lighting.setDirectional(sun);

    engine::lighting::AmbientSettings amb;
    amb.skyZenith = dn.skyZenith;
    amb.skyHorizon = dn.skyHorizon;
    amb.dayLight = dn.dayLight;
    amb.hemisphereStrength = 0.42f;
    lighting.setAmbient(amb);

    lighting.buildPointLightsFromWorld(world, cam, 72.0f);
    lighting.uploadToShader(sh);
}

} // namespace

void VoxelApp_framebufferSize(GLFWwindow* w, int width, int height) {
    if (auto* a = static_cast<VoxelApplication*>(glfwGetWindowUserPointer(w)))
        a->onFramebufferSize(width, height);
}

void VoxelApp_key(GLFWwindow* w, int key, int, int action, int) {
    auto* a = static_cast<VoxelApplication*>(glfwGetWindowUserPointer(w));
    if (!a)
        return;
    const bool down = action == GLFW_PRESS || action == GLFW_REPEAT;
    if (key == GLFW_KEY_W)
        a->keyW_ = down;
    if (key == GLFW_KEY_A)
        a->keyA_ = down;
    if (key == GLFW_KEY_S)
        a->keyS_ = down;
    if (key == GLFW_KEY_D)
        a->keyD_ = down;
    if (key == GLFW_KEY_SPACE)
        a->keySpace_ = down;
    if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
        a->keyShift_ = down;
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(w, 1);
        if (key == GLFW_KEY_H) {
            const int ax = a->villageAnchorWx_;
            const int az = a->villageAnchorWz_;
            const int h = voxel::VoxelWorld::terrainHeight(ax, az);
            a->camPos_ = glm::vec3(static_cast<float>(ax) + 0.5f, static_cast<float>(h) + 3.5f, static_cast<float>(az) + 0.5f);
            const float rdx = static_cast<float>(ax) - 11.0f - a->camPos_.x;
            const float rdz = static_cast<float>(az) - 5.0f - a->camPos_.z;
            a->yawDeg_ = glm::degrees(std::atan2(rdz, rdx));
            a->player_.setEyePosition(a->camPos_);
            a->pushToast("Returned to village square");
        }
        if (key == GLFW_KEY_F11)
            a->toggleFullscreen();
        if (key == GLFW_KEY_TAB) {
            a->mouseCaptured_ = !a->mouseCaptured_;
            glfwSetInputMode(w, GLFW_CURSOR,
                              a->mouseCaptured_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (a->mouseCaptured_)
                a->firstMouse_ = true;
        }
        if (key == GLFW_KEY_F)
            a->walkMode_ = !a->walkMode_;
        if (key == GLFW_KEY_E)
            a->tryInteract();
        if (key == GLFW_KEY_G)
            a->tryPickBlock();
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
            a->setHotbarIndex(key - GLFW_KEY_1);
        if (key == GLFW_KEY_0)
            a->setHotbarIndex(9);
        if (key == GLFW_KEY_F1)
            a->setHotbarIndex(9);
        if (key == GLFW_KEY_F2) {
            a->hotbar_[9] = voxel::BlockId::LampCyan;
            a->setHotbarIndex(9);
        }
        if (key == GLFW_KEY_F3) {
            a->hotbar_[9] = voxel::BlockId::LampMagenta;
            a->setHotbarIndex(9);
        }
    }
}

void VoxelApp_scroll(GLFWwindow* w, double, double yoff) {
    auto* a = static_cast<VoxelApplication*>(glfwGetWindowUserPointer(w));
    if (!a || !a->mouseCaptured_)
        return;
    if (yoff > 0.0)
        a->setHotbarIndex(a->hotbarIndex_ - 1);
    else if (yoff < 0.0)
        a->setHotbarIndex(a->hotbarIndex_ + 1);
}

void VoxelApp_cursor(GLFWwindow* w, double x, double y) {
    auto* a = static_cast<VoxelApplication*>(glfwGetWindowUserPointer(w));
    if (!a || !a->mouseCaptured_)
        return;
    if (a->firstMouse_) {
        a->lastMouseX_ = x;
        a->lastMouseY_ = y;
        a->firstMouse_ = false;
        return;
    }
    const double dx = x - a->lastMouseX_;
    const double dy = a->lastMouseY_ - y;
    a->lastMouseX_ = x;
    a->lastMouseY_ = y;
    a->yawDeg_ += static_cast<float>(dx) * a->mouseSens_;
    a->pitchDeg_ += static_cast<float>(dy) * a->mouseSens_;
    if (a->pitchDeg_ > 89.0f)
        a->pitchDeg_ = 89.0f;
    if (a->pitchDeg_ < -89.0f)
        a->pitchDeg_ = -89.0f;
}

void VoxelApp_mouseButton(GLFWwindow* w, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_MIDDLE || action != GLFW_PRESS)
        return;
    if (auto* a = static_cast<VoxelApplication*>(glfwGetWindowUserPointer(w)))
        a->toggleFullscreen();
}

void VoxelApplication::toggleFullscreen() {
    GLFWwindow* w = window_.native();
    if (!w)
        return;
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    if (!mon)
        return;
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    if (!mode)
        return;
    int mx = 0;
    int my = 0;
    glfwGetMonitorPos(mon, &mx, &my);

    if (!borderlessMaximized_) {
        glfwGetWindowPos(w, &winRestoreX_, &winRestoreY_);
        glfwGetWindowSize(w, &winRestoreW_, &winRestoreH_);
        glfwSetWindowPos(w, mx, my);
        glfwSetWindowSize(w, mode->width, mode->height);
        borderlessMaximized_ = true;
    } else {
        glfwSetWindowPos(w, winRestoreX_, winRestoreY_);
        glfwSetWindowSize(w, winRestoreW_, winRestoreH_);
        borderlessMaximized_ = false;
    }
}

void VoxelApplication::onFramebufferSize(int w, int h) {
    if (w <= 0 || h <= 0)
        return;
    fbW_ = w;
    fbH_ = h;
    glViewport(0, 0, w, h);
    recreateSceneFbo(w, h);
}

bool VoxelApplication::intersectsPlayer(const glm::ivec3& block) const {
    const glm::vec3 bmin(static_cast<float>(block.x), static_cast<float>(block.y), static_cast<float>(block.z));
    const glm::vec3 bmax = bmin + 1.0f;
    const glm::vec3 feet = camPos_ - glm::vec3(0.0f, 1.62f, 0.0f);
    const glm::vec3 pmin = feet - glm::vec3(0.28f, 0.05f, 0.28f);
    const glm::vec3 pmax = feet + glm::vec3(0.28f, 1.82f, 0.28f);
    return pmin.x < bmax.x && pmax.x > bmin.x && pmin.y < bmax.y && pmax.y > bmin.y && pmin.z < bmax.z && pmax.z > bmin.z;
}

void VoxelApplication::destroyShadowMap() {
    if (shadowDepthTex_) {
        glDeleteTextures(1, &shadowDepthTex_);
        shadowDepthTex_ = 0;
    }
    if (shadowFbo_) {
        glDeleteFramebuffers(1, &shadowFbo_);
        shadowFbo_ = 0;
    }
}

void VoxelApplication::createShadowMap() {
    destroyShadowMap();
    glGenFramebuffers(1, &shadowFbo_);
    glGenTextures(1, &shadowDepthTex_);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, kShadowMapSize_, kShadowMapSize_, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowMapSize_, kShadowMapSize_, 0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, nullptr);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex_, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "VoxelApplication: shadow FBO incomplete (32F and 24 fallback)\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VoxelApplication::destroySceneFbo() {
    if (sceneDepthRbo_) {
        glDeleteRenderbuffers(1, &sceneDepthRbo_);
        sceneDepthRbo_ = 0;
    }
    if (sceneColorTex_) {
        glDeleteTextures(1, &sceneColorTex_);
        sceneColorTex_ = 0;
    }
    if (sceneFbo_) {
        glDeleteFramebuffers(1, &sceneFbo_);
        sceneFbo_ = 0;
    }
}

void VoxelApplication::destroyRefrFbo() {
    if (refrColorTex_) {
        glDeleteTextures(1, &refrColorTex_);
        refrColorTex_ = 0;
    }
    if (refrFbo_) {
        glDeleteFramebuffers(1, &refrFbo_);
        refrFbo_ = 0;
    }
    refrW_ = 0;
    refrH_ = 0;
}

void VoxelApplication::recreateRefrFbo() {
    destroyRefrFbo();
    if (fbW_ <= 0 || fbH_ <= 0)
        return;
    refrW_ = std::max(1, fbW_ / 2);
    refrH_ = std::max(1, fbH_ / 2);
    glGenFramebuffers(1, &refrFbo_);
    glGenTextures(1, &refrColorTex_);
    glBindTexture(GL_TEXTURE_2D, refrColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, refrW_, refrH_, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, refrFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, refrColorTex_, 0);
    const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "VoxelApplication: refract FBO incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void VoxelApplication::recreateSceneFbo(int w, int h) {
    destroyRefrFbo();
    destroySceneFbo();
    glGenFramebuffers(1, &sceneFbo_);
    glGenTextures(1, &sceneColorTex_);
    glGenRenderbuffers(1, &sceneDepthRbo_);

    glBindTexture(GL_TEXTURE_2D, sceneColorTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTex_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepthRbo_);
    const GLenum bufs[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, bufs);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "VoxelApplication: scene FBO incomplete\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    recreateRefrFbo();
}

void VoxelApplication::destroyHudGeometry() {
    if (blockOutlineVbo_) {
        glDeleteBuffers(1, &blockOutlineVbo_);
        blockOutlineVbo_ = 0;
    }
    if (blockOutlineVao_) {
        glDeleteVertexArrays(1, &blockOutlineVao_);
        blockOutlineVao_ = 0;
    }
    if (hudQuadVbo_) {
        glDeleteBuffers(1, &hudQuadVbo_);
        hudQuadVbo_ = 0;
    }
    if (hudQuadVao_) {
        glDeleteVertexArrays(1, &hudQuadVao_);
        hudQuadVao_ = 0;
    }
    if (crosshairVbo_) {
        glDeleteBuffers(1, &crosshairVbo_);
        crosshairVbo_ = 0;
    }
    if (crosshairVao_) {
        glDeleteVertexArrays(1, &crosshairVao_);
        crosshairVao_ = 0;
    }
}

void VoxelApplication::initHudGeometry() {
    destroyHudGeometry();
    const float gap = 0.012f;
    const float arm = 0.05f;
    const float crossVerts[] = {
        -arm, 0.0f, 0.0f, -gap, 0.0f, 0.0f, gap, 0.0f, 0.0f, arm, 0.0f, 0.0f,
        0.0f, -arm, 0.0f, 0.0f, -gap, 0.0f, 0.0f, gap, 0.0f, 0.0f, arm, 0.0f,
    };
    glGenVertexArrays(1, &crosshairVao_);
    glGenBuffers(1, &crosshairVbo_);
    glBindVertexArray(crosshairVao_);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crossVerts), crossVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &hudQuadVao_);
    glGenBuffers(1, &hudQuadVbo_);
    glBindVertexArray(hudQuadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudQuadVbo_);
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const float cubeEdges[] = {
        0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0,
        0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0,
        0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 1, 1,
    };
    glGenVertexArrays(1, &blockOutlineVao_);
    glGenBuffers(1, &blockOutlineVbo_);
    glBindVertexArray(blockOutlineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, blockOutlineVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeEdges), cubeEdges, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VoxelApplication::setHotbarIndex(int idx) {
    hotbarIndex_ = (idx % kHotbarSlots_ + kHotbarSlots_) % kHotbarSlots_;
    placeBlock_ = hotbar_[hotbarIndex_];
}

void VoxelApplication::pushToast(const std::string& msg, float durationSec) {
    toastMsg_ = msg;
    toastRemainSec_ = durationSec;
}

void VoxelApplication::updateAimTarget() {
    aimValid_ = false;
    if (!mouseCaptured_)
        return;
    const glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
    glm::ivec3 hit, hn;
    if (!world_.raycastBlock(camPos_, front, 8.0f, hit, hn))
        return;
    aimValid_ = true;
    aimBlock_ = hit;
    aimNormal_ = hn;
    aimDist_ = glm::length(glm::vec3(hit) + glm::vec3(0.5f) - camPos_);

    interactHint_ = false;
    const voxel::BlockId b = world_.getBlock(hit.x, hit.y, hit.z);
    if (voxel::isLamp(b) || b == voxel::BlockId::HayThatch || b == voxel::BlockId::PathStone ||
        b == voxel::BlockId::Water || b == voxel::BlockId::RoofTerracotta)
        interactHint_ = true;

    const glm::vec3 frontFlat = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    for (const voxel::SceneryFlag& f : world_.sceneryFlags()) {
        const glm::vec3 fp(f.poleX, f.clothAnchorY, f.poleZ);
        if (glm::length(fp - camPos_) > 6.5f)
            continue;
        const glm::vec3 to = fp - camPos_;
        if (glm::dot(frontFlat, glm::normalize(glm::vec3(to.x, 0.0f, to.z))) > 0.45f) {
            interactHint_ = true;
            break;
        }
    }
}

void VoxelApplication::tryPickBlock() {
    if (!aimValid_)
        return;
    const voxel::BlockId b = world_.getBlock(aimBlock_.x, aimBlock_.y, aimBlock_.z);
    if (!voxel::isSolid(b) || b == voxel::BlockId::Water)
        return;
    hotbar_[hotbarIndex_] = b;
    placeBlock_ = b;
    pushToast(std::string("Picked: ") + blockNameEn(b));
}

void VoxelApplication::tryInteract() {
    if (!mouseCaptured_)
        return;

    const glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
    const glm::vec3 frontFlat = glm::length(glm::vec3(front.x, 0.0f, front.z)) > 1e-4f
                                    ? glm::normalize(glm::vec3(front.x, 0.0f, front.z))
                                    : glm::vec3(0.0f, 0.0f, 1.0f);

    float bestFlagD2 = 36.0f;
    const voxel::SceneryFlag* bestFlag = nullptr;
    for (const voxel::SceneryFlag& f : world_.sceneryFlags()) {
        const glm::vec3 fp(f.poleX, f.clothAnchorY, f.poleZ);
        const glm::vec3 to = fp - camPos_;
        const float d2 = glm::dot(to, to);
        if (d2 > 36.0f)
            continue;
        if (glm::dot(frontFlat, glm::normalize(glm::vec3(to.x, 0.0f, to.z))) < 0.35f)
            continue;
        if (d2 < bestFlagD2) {
            bestFlagD2 = d2;
            bestFlag = &f;
        }
    }
    if (bestFlag) {
        pushToast("Village banner ripples in the breeze");
        exploreScore_ += 1;
        return;
    }

    if (!aimValid_)
        return;
    const voxel::BlockId b = world_.getBlock(aimBlock_.x, aimBlock_.y, aimBlock_.z);
    if (voxel::isLamp(b)) {
        if (keyShift_) {
            world_.setBlock(aimBlock_.x, aimBlock_.y, aimBlock_.z, voxel::BlockId::Wood);
            pushToast("Street lamp switched off");
        } else {
            const voxel::BlockId next = cycleLamp(b);
            world_.setBlock(aimBlock_.x, aimBlock_.y, aimBlock_.z, next);
            pushToast(std::string("Lamp: ") + blockNameEn(next));
        }
        return;
    }
    switch (b) {
    case voxel::BlockId::Water:
        pushToast("Clear spring water — cool and fresh");
        break;
    case voxel::BlockId::HayThatch:
        pushToast("Dry thatch — smells of hay and smoke");
        break;
    case voxel::BlockId::PathStone:
        pushToast("Worn cobble — villagers walked here for years");
        break;
    case voxel::BlockId::RoofTerracotta:
        pushToast("Terracotta tiles warm in the sun");
        break;
    case voxel::BlockId::Wood:
        if (world_.getBlock(aimBlock_.x, aimBlock_.y + 1, aimBlock_.z) == voxel::BlockId::Air &&
            world_.getBlock(aimBlock_.x, aimBlock_.y + 2, aimBlock_.z) == voxel::BlockId::Air) {
            world_.setBlock(aimBlock_.x, aimBlock_.y + 1, aimBlock_.z, voxel::BlockId::LampWarm);
            pushToast("Relit street lamp");
        } else {
            pushToast("Sturdy timber post");
        }
        break;
    default:
        pushToast(std::string("Inspect: ") + blockNameEn(b));
        break;
    }
}

void VoxelApplication::updateExploration(float dt) {
    (void)dt;
    for (int i = 0; i < kLandmarkCount_; ++i) {
        if (landmarkFound_[i])
            continue;
        if (glm::length(camPos_ - landmarkPos_[i]) > 10.0f)
            continue;
        landmarkFound_[i] = true;
        exploreScore_ += 3;
        pushToast(std::string("Discovered: ") + landmarkName_[i]);
    }
}

void VoxelApplication::updatePlayerMovement(float dt) {
    if (!collider_)
        return;
    glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 move(0.0f);
    if (keyW_)
        move += front;
    if (keyS_)
        move -= front;
    if (keyD_)
        move += right;
    if (keyA_)
        move -= right;

    engine::physics::CharacterInput in;
    in.wishDirWorld = move;
    in.jump = keySpace_;
    in.sprint = keyShift_;
    in.flyMode = !walkMode_;
    in.flyUp = keySpace_ ? 1.0f : 0.0f;
    in.flyDown = keyShift_ ? 1.0f : 0.0f;

    player_.tick(*collider_, dt, in);
    camPos_ = player_.eyePosition();
}

bool VoxelApplication::init() {
    if (!window_.create(voxel::kWindowW, voxel::kWindowH, "GameEigen Voxel")) {
        std::cerr << "VoxelApplication: window create failed\n";
        return false;
    }
    GLFWwindow* w = window_.native();
    glfwSetWindowUserPointer(w, this);
    glfwSetFramebufferSizeCallback(w, VoxelApp_framebufferSize);
    glfwSetKeyCallback(w, VoxelApp_key);
    glfwSetCursorPosCallback(w, VoxelApp_cursor);
    glfwSetMouseButtonCallback(w, VoxelApp_mouseButton);
    glfwSetScrollCallback(w, VoxelApp_scroll);
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwGetWindowPos(w, &winRestoreX_, &winRestoreY_);
    glfwGetWindowSize(w, &winRestoreW_, &winRestoreH_);

    window_.framebufferSize(fbW_, fbH_);
    if (fbW_ <= 0 || fbH_ <= 0) {
        fbW_ = voxel::kWindowW;
        fbH_ = voxel::kWindowH;
    }
    glViewport(0, 0, fbW_, fbH_);
    recreateSceneFbo(fbW_, fbH_);

    const std::string sd = shaderDir();
    try {
        chunkShader_ = std::make_unique<Shader>((sd + "voxel_chunk.vs").c_str(), (sd + "voxel_chunk.fs").c_str());
        depthShader_ = std::make_unique<Shader>((sd + "voxel_depth.vs").c_str(), (sd + "voxel_depth.fs").c_str());
        waterShader_ = std::make_unique<Shader>((sd + "voxel_water.vs").c_str(), (sd + "voxel_water.fs").c_str());
        hudShader_ = std::make_unique<Shader>((sd + "unlit_color.vs").c_str(), (sd + "unlit_color.fs").c_str());
        skyShader_ = std::make_unique<Shader>((sd + "sky_gradient.vs").c_str(), (sd + "sky_gradient.fs").c_str());
        flagShader_ = std::make_unique<Shader>((sd + "scenery_flag.vs").c_str(), (sd + "scenery_flag.fs").c_str());
        flagDepthShader_ =
            std::make_unique<Shader>((sd + "scenery_flag_depth.vs").c_str(), (sd + "scenery_flag_depth.fs").c_str());
    } catch (...) {
        std::cerr << "VoxelApplication: shader load failed\n";
        return false;
    }

    createShadowMap();
    if (!voxelAtlas_.init()) {
        std::cerr << "VoxelApplication: atlas init failed\n";
        destroyShadowMap();
        return false;
    }

    waterShader_->use();
    waterShader_->setInt("sceneColor", 0);

    chunkShader_->use();
    chunkShader_->setInt("uGlassOnly", 0);
    chunkShader_->setInt("uAtlas", 0);
    chunkShader_->setInt("uShadowMap", 1);
    chunkShader_->setInt("uSceneRefr", 2);
    chunkShader_->setInt("uAtlasN", 3);
    {
        const float invSh = 1.0f / static_cast<float>(kShadowMapSize_);
        chunkShader_->setVec2("uShadowTexelSize", glm::vec2(invSh, invSh));
    }

    flagShader_->use();
    flagShader_->setInt("uShadowMap", 1);

    rigidSky_.initUnitSphere();
    scenery_.init();
    smokePlumes_.init();
    scaledBlocks_.init();

    initHudGeometry();
    setHotbarIndex(hotbarIndex_);

    if (!engine_.init()) {
        std::cerr << "VoxelApplication: engine init failed\n";
        return false;
    }
    collider_ = std::make_unique<engine::physics::VoxelCollider>(world_);
    engine_.physics().setCollider(collider_.get());
    world_.setSceneLighting(&engine_.lighting());

    game::GameAPI::get().bind(&engine_, &world_, &player_, collider_.get());
    game::GameAPI::get().setToastHandler([this](const std::string& msg, float dur) { pushToast(msg, dur); });

    const int sx = 24;
    const int sz = 22;
    villageAnchorWx_ = sx;
    villageAnchorWz_ = sz;
    world_.streamAround(sx, sz, kStreamHalf_);
    const int th = voxel::VoxelWorld::terrainHeight(sx, sz);
    int mox = sx + 40;
    int moz = sz + 40;
    {
        const std::string voxPath = std::string(logl_root) + "/assets/h_model.voxdat";
        if (world_.placeFromVoxDat(voxPath.c_str(), mox, th + 1, moz, voxel::BlockId::Brick)) {
            world_.streamAround(mox + 24, moz + 24, 12);
        } else {
            std::cerr << "[GameEigen] 未找到体素数据（可忽略或生成）: " << voxPath
                      << "\n  生成命令: python tools/voxelize_obj_to_voxdat.py\n";
        }
    }
    const int bridgeX = sx - 11;
    const int bridgeZ = sz - 5;
    const int bridgeH = voxel::VoxelWorld::terrainHeight(bridgeX, bridgeZ);
    landmarkPos_[0] = glm::vec3(static_cast<float>(sx) + 0.5f, static_cast<float>(th) + 2.0f,
                                static_cast<float>(sz) + 0.5f);
    landmarkPos_[1] = glm::vec3(static_cast<float>(bridgeX) + 0.5f, static_cast<float>(bridgeH) + 3.5f,
                               static_cast<float>(bridgeZ) + 0.5f);
    landmarkPos_[2] = glm::vec3(static_cast<float>(mox) + 24.0f, static_cast<float>(th) + 10.0f,
                               static_cast<float>(moz) + 24.0f);
    landmarkPos_[3] = glm::vec3(static_cast<float>(sx) + 18.0f, static_cast<float>(th) + 8.0f,
                               static_cast<float>(sz) - 14.0f);
    landmarkName_[0] = "Village Square";
    landmarkName_[1] = "Wooden Bridge";
    landmarkName_[2] = "Stone Monument";
    landmarkName_[3] = "Chimney Smoke";
    landmarkFound_.fill(false);
    exploreScore_ = 0;
    pushToast("Welcome — explore the village! E interact, G pick block, F toggle fly", 5.0f);

    camPos_ = glm::vec3(static_cast<float>(sx) + 0.5f, static_cast<float>(th) + 3.5f, static_cast<float>(sz) + 0.5f);
    player_.setEyePosition(camPos_);
    /* 默认朝向木桥/小径一侧，避免出生在锚点上却背对村落 */
    {
        const float lookWx = static_cast<float>(sx) - 11.0f;
        const float lookWz = static_cast<float>(sz) - 5.0f;
        const float rdx = lookWx - camPos_.x;
        const float rdz = lookWz - camPos_.z;
        yawDeg_ = glm::degrees(std::atan2(rdz, rdx));
    }
    pitchDeg_ = -12.0f;
    streamPrimed_ = true;
    streamCx_ = floorChunkDiv(sx);
    streamCz_ = floorChunkDiv(sz);
    streamWarmupRemainSec_ = 3.2f;

    return true;
}

void VoxelApplication::shutdown() {
    game::GameAPI::get().bind(nullptr, nullptr, nullptr, nullptr);
    engine_.shutdown();
    collider_.reset();
    destroyHudGeometry();
    scenery_.shutdown();
    smokePlumes_.shutdown();
    scaledBlocks_.shutdown();
    rigidSky_.shutdown();
    voxelAtlas_.shutdown();
    destroyShadowMap();
    destroyRefrFbo();
    destroySceneFbo();
    chunkShader_.reset();
    depthShader_.reset();
    waterShader_.reset();
    hudShader_.reset();
    skyShader_.reset();
    flagShader_.reset();
    flagDepthShader_.reset();
    window_.destroy();
}

void VoxelApplication::update(float dt) {
    GLFWwindow* w = window_.native();

    engine_.beginFrame(dt);
    updatePlayerMovement(dt);
    updateAimTarget();
    updateExploration(dt);
    if (toastRemainSec_ > 0.0f)
        toastRemainSec_ = std::max(0.0f, toastRemainSec_ - dt);

    const glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
    const int wx = static_cast<int>(std::floor(camPos_.x));
    const int wz = static_cast<int>(std::floor(camPos_.z));
    const int ncx = floorChunkDiv(wx);
    const int ncz = floorChunkDiv(wz);
    if (!streamPrimed_ || ncx != streamCx_ || ncz != streamCz_) {
        world_.streamAround(wx, wz, kStreamHalf_);
        streamCx_ = ncx;
        streamCz_ = ncz;
        streamPrimed_ = true;
    }
    /* 每帧按相机裁剪：避免 chunks_ / pendingCreate_ 随探索无限增长 */
    world_.pruneChunksAwayFromCamera(camPos_, kStreamHalf_);

    int spawnBudget = voxel::kChunkCreateBudgetPerFrame;
    if (streamWarmupRemainSec_ > 0.0f) {
        streamWarmupRemainSec_ -= dt;
        spawnBudget += 8;
    }
    spawnBudget += static_cast<int>(std::min(world_.pendingChunkCreateCount() / 18, static_cast<size_t>(12)));
    spawnBudget = std::min(spawnBudget, 28);
    world_.pumpPendingChunkCreations(spawnBudget, camPos_);

    world_.rebuildDirtyMeshes(voxel::kMeshRebuildBudgetPerFrame, &camPos_, voxel::kMeshRebuildCpuBudgetSec);
    world_.uploadMeshes(voxel::kMeshUploadBudgetPerFrame, &camPos_, voxel::kMeshUploadCpuBudgetSec);

    const bool ml = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const bool mr = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    const bool breakEdge = ml && !mouseLPrev_;
    const bool placeEdge = mr && !mouseRPrev_;
    mouseLPrev_ = ml;
    mouseRPrev_ = mr;

    glm::ivec3 hit, hn;
    const glm::vec3 eye = camPos_;
    if (mouseCaptured_ && breakEdge && world_.raycastBlock(eye, front, 10.0f, hit, hn))
        world_.setBlock(hit.x, hit.y, hit.z, voxel::BlockId::Air);
    if (mouseCaptured_ && placeEdge && world_.raycastBlock(eye, front, 10.0f, hit, hn)) {
        const glm::ivec3 place = hit + hn;
        if (glm::length(glm::vec3(place) + glm::vec3(0.5f) - eye) < 9.0f && !intersectsPlayer(place)) {
            const voxel::BlockId cur = world_.getBlock(place.x, place.y, place.z);
            if (voxel::isReplaceableWhenPlacing(cur))
                world_.setBlock(place.x, place.y, place.z, placeBlock_);
        }
    }

    titleIntervalAccum_ += dt;
    if (titleIntervalAccum_ >= 0.25f) {
        titleIntervalAccum_ = 0.0f;
        int found = 0;
        for (bool f : landmarkFound_)
            if (f)
                ++found;
        std::string title = "GameEigen Voxel | ";
        title += blockNameEn(placeBlock_);
        title += " | Explore ";
        title += std::to_string(found);
        title += "/";
        title += std::to_string(kLandmarkCount_);
        title += " score ";
        title += std::to_string(exploreScore_);
        title += walkMode_ ? " | Walk" : " | Fly";
        if (!toastMsg_.empty() && toastRemainSec_ > 0.0f) {
            title += " | ";
            title += toastMsg_;
        }
        glfwSetWindowTitle(w, title.c_str());
    }
}

void VoxelApplication::drawCrosshair() {
    if (!hudShader_ || crosshairVao_ == 0)
        return;
    glDisable(GL_DEPTH_TEST);
    hudShader_->use();
    hudShader_->setMat4("mvp", glm::mat4(1.0f));
    hudShader_->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
    hudShader_->setFloat("uAlpha", 1.0f);
    glLineWidth(2.0f);
    glBindVertexArray(crosshairVao_);
    glDrawArrays(GL_LINES, 0, 8);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void VoxelApplication::drawHud(float timeSec) {
    if (!hudShader_ || hudQuadVao_ == 0)
        return;

    auto drawNdcQuad = [&](float x0, float y0, float x1, float y1, const glm::vec3& col, float alpha) {
        const float verts[] = {x0, y0, 0.0f, x1, y0, 0.0f, x1, y1, 0.0f, x0, y1, 0.0f};
        glBindBuffer(GL_ARRAY_BUFFER, hudQuadVbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        hudShader_->use();
        hudShader_->setMat4("mvp", glm::mat4(1.0f));
        hudShader_->setVec3("color", col);
        hudShader_->setFloat("uAlpha", alpha);
        glBindVertexArray(hudQuadVao_);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
    };

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    constexpr float slotW = 0.068f;
    constexpr float slotH = 0.095f;
    constexpr float gap = 0.008f;
    const float totalW = kHotbarSlots_ * slotW + (kHotbarSlots_ - 1) * gap;
    float x = -totalW * 0.5f;
    const float y0 = -0.92f;
    const float y1 = y0 + slotH;

    drawNdcQuad(-0.52f, -0.96f, 0.52f, -0.84f, glm::vec3(0.04f, 0.05f, 0.08f), 0.72f);

    for (int i = 0; i < kHotbarSlots_; ++i) {
        const float x0 = x;
        const float x1 = x + slotW;
        const bool sel = i == hotbarIndex_;
        drawNdcQuad(x0 - 0.003f, y0 - 0.008f, x1 + 0.003f, y1 + 0.008f,
                    sel ? glm::vec3(0.95f, 0.82f, 0.35f) : glm::vec3(0.22f, 0.24f, 0.28f), sel ? 0.95f : 0.55f);
        drawNdcQuad(x0, y0, x1, y1, blockHudColor(hotbar_[i]), 0.92f);
        x = x1 + gap;
    }

    if (interactHint_) {
        const float pulse = 0.55f + 0.25f * std::sin(timeSec * 5.5f);
        drawNdcQuad(0.78f, -0.08f, 0.92f, 0.08f, glm::vec3(0.35f, 0.95f, 0.82f), pulse);
        drawNdcQuad(0.805f, -0.035f, 0.845f, 0.035f, glm::vec3(0.02f, 0.08f, 0.06f), 0.95f);
    }

    if (toastRemainSec_ > 0.0f && !toastMsg_.empty()) {
        const float fade = glm::clamp(toastRemainSec_ / 0.6f, 0.35f, 1.0f);
        drawNdcQuad(-0.72f, 0.78f, 0.72f, 0.9f, glm::vec3(0.06f, 0.08f, 0.12f), 0.78f * fade);
        drawNdcQuad(-0.70f, 0.80f, -0.68f + 0.04f * fade, 0.88f, glm::vec3(0.45f, 0.88f, 0.72f), fade);
    }

    int found = 0;
    for (bool f : landmarkFound_)
        if (f)
            ++found;
    const float prog = static_cast<float>(found) / static_cast<float>(kLandmarkCount_);
    drawNdcQuad(-0.72f, 0.68f, 0.72f, 0.72f, glm::vec3(0.12f, 0.14f, 0.18f), 0.65f);
    drawNdcQuad(-0.72f, 0.68f, -0.72f + 1.44f * prog, 0.72f, glm::vec3(0.38f, 0.72f, 0.95f), 0.9f);

    if (walkMode_ && player_.inWater()) {
        drawNdcQuad(-0.15f, 0.55f, 0.15f, 0.62f, glm::vec3(0.2f, 0.55f, 0.95f), 0.55f);
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void VoxelApplication::drawSky(const glm::mat4& proj, const glm::mat4& view, float timeSec, const glm::vec3& sunSkyWorld,
                               const glm::vec3& horizon, const glm::vec3& zenith, float dayLight) {
    if (!skyShader_)
        return;

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);

    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), camPos_) * glm::scale(glm::mat4(1.0f), glm::vec3(220.0f));

    skyShader_->use();
    skyShader_->setMat4("uProj", proj);
    skyShader_->setMat4("uView", view);
    skyShader_->setMat4("uModel", model);
    skyShader_->setVec3("uCamPos", camPos_);
    skyShader_->setVec3("uHorizon", horizon);
    skyShader_->setVec3("uZenith", zenith);
    skyShader_->setVec3("uSunDirWorld", sunSkyWorld);
    skyShader_->setFloat("uSunBloom", 0.72f + 0.07f * std::sin(timeSec * 0.35f));
    skyShader_->setFloat("uDayLight", dayLight);

    rigidSky_.drawSphere();

    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void VoxelApplication::render(float timeSec) {
    const float aspect = fbH_ > 0 ? static_cast<float>(fbW_) / static_cast<float>(fbH_) : 1.0f;
    const glm::mat4 proj = glm::perspective(glm::radians(64.0f), aspect, 0.1f, 320.0f);
    glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
    const glm::mat4 view = glm::lookAt(camPos_, camPos_ + front, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 vp = proj * view;

    const DayNight dn = computeDayNight(timeSec);
    const glm::vec3 toSun = glm::normalize(-dn.shadeSunDir);
    const glm::vec3 shadowCenter = camPos_;
    const glm::vec3 shadowEye = shadowCenter + toSun * 128.0f;
    glm::mat4 lightView = glm::lookAt(shadowEye, shadowCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    const float orthoExtent = 102.0f;
    const glm::mat4 lightProj = glm::ortho(-orthoExtent, orthoExtent, -orthoExtent, orthoExtent, 0.5f, 280.0f);
    glm::mat4 lightSpaceMatrix = lightProj * lightView;

    /* 正交阴影矩阵按 texel 对齐，减轻相机移动时阴影边缘抖动 */
    {
        const float texWorld = (2.0f * orthoExtent) / static_cast<float>(kShadowMapSize_);
        glm::vec4 cLight = lightView * glm::vec4(shadowCenter, 1.0f);
        cLight.x = std::floor(cLight.x / texWorld) * texWorld;
        cLight.y = std::floor(cLight.y / texWorld) * texWorld;
        const glm::mat4 invLv = glm::inverse(lightView);
        const glm::vec3 snapped = glm::vec3(invLv * cLight);
        lightView = glm::lookAt(shadowEye, snapped, glm::vec3(0.0f, 1.0f, 0.0f));
        lightSpaceMatrix = lightProj * lightView;
    }

    glViewport(0, 0, kShadowMapSize_, kShadowMapSize_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.15f, 850.0f);
    depthShader_->use();
    depthShader_->setMat4("uLightMvp", lightSpaceMatrix);
    for (const auto& kv : world_.chunks()) {
        const voxel::Chunk* ch = kv.second.get();
        if (!chunkNearShadowCastDistance(ch, shadowCenter, orthoExtent))
            continue;
        if (ch->hasOpaque())
            ch->drawOpaque();
        if (ch->hasGlass())
            ch->drawGlass();
    }
    if (scenery_.valid() && flagDepthShader_) {
        /* 薄片双面进深度图，避免阴影 pass 使用 GL_FRONT 剔除时旗面被吃掉 */
        glDisable(GL_CULL_FACE);
        flagDepthShader_->use();
        flagDepthShader_->setMat4("uLightMvp", lightSpaceMatrix);
        flagDepthShader_->setFloat("uTime", timeSec);
        for (const voxel::SceneryFlag& f : world_.sceneryFlags()) {
            const glm::mat4 model =
                glm::translate(glm::mat4(1.0f), glm::vec3(f.poleX, f.clothAnchorY, f.poleZ)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(f.yawDeg), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(2.85f, 2.85f, 1.0f));
            flagDepthShader_->setMat4("uModel", model);
            flagDepthShader_->setFloat("uPhase", f.poleX * 0.31f + f.poleZ * 0.19f);
            scenery_.drawElements();
        }
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const float ew = eyeInWaterFactor(world_, camPos_);
    const glm::vec3 skyZenith = dn.skyZenith;
    const glm::vec3 skyHorizon = dn.skyHorizon;
    const glm::vec3 clearRgb =
        glm::mix(skyHorizon * 0.92f, glm::vec3(0.06f, 0.14f, 0.24f), ew * 0.65f);

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, fbW_, fbH_);
    glClearColor(clearRgb.r, clearRgb.g, clearRgb.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::vec3 sunSkyWorld = glm::normalize(-dn.visualSunDir);
    drawSky(proj, view, timeSec, sunSkyWorld, skyHorizon, skyZenith, dn.dayLight);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    chunkShader_->use();
    chunkShader_->setMat4("uMvp", vp);
    chunkShader_->setMat4("uLightSpaceMatrix", lightSpaceMatrix);
    chunkShader_->setVec2("uShadowTexelSize",
                          glm::vec2(1.0f / static_cast<float>(kShadowMapSize_), 1.0f / static_cast<float>(kShadowMapSize_)));
    chunkShader_->setVec3("uSunDir", dn.shadeSunDir);
    chunkShader_->setVec3("uViewPos", camPos_);
    chunkShader_->setFloat("uEyeInWater", ew);
    chunkShader_->setFloat("uTime", timeSec);
    chunkShader_->setInt("uGlassOnly", 0);
    chunkShader_->setVec3("uSkyZenith", skyZenith);
    chunkShader_->setVec3("uSkyHorizon", skyHorizon);
    chunkShader_->setFloat("uDayLight", dn.dayLight);
    syncLightingFromScene(*chunkShader_, engine_.lighting(), world_, camPos_, dn);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, voxelAtlas_.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, voxelAtlas_.normalTexture);
    chunkShader_->setInt("uHasAtlasN", voxelAtlas_.normalTexture ? 1 : 0);

    /* 室内从下往上看顶面时外法线朝外会变成背面；体素不透明/玻璃/水绘制期间关闭背面剔除 */
    glDisable(GL_CULL_FACE);
    for (const auto& kv : world_.chunks()) {
        const voxel::Chunk* ch = kv.second.get();
        if (ch->hasOpaque() && chunkInDrawRange(ch, camPos_))
            ch->drawOpaque();
    }

    if (scaledBlocks_.valid() && !world_.scaledVoxels().empty()) {
        scaledBlocks_.applyLighting(engine_.lighting());
        scaledBlocks_.draw(vp, lightSpaceMatrix, dn.shadeSunDir, camPos_, skyZenith, skyHorizon, dn.dayLight,
                           voxelAtlas_.texture, shadowDepthTex_, 1.0f / static_cast<float>(kShadowMapSize_),
                           world_.scaledVoxels());
    }

    if (scenery_.valid() && flagShader_) {
        flagShader_->use();
        flagShader_->setMat4("uMvp", vp);
        flagShader_->setMat4("uLightSpaceMatrix", lightSpaceMatrix);
        flagShader_->setVec2("uShadowTexelSize",
                             glm::vec2(1.0f / static_cast<float>(kShadowMapSize_), 1.0f / static_cast<float>(kShadowMapSize_)));
        flagShader_->setVec3("uSunDir", dn.shadeSunDir);
        flagShader_->setVec3("uViewPos", camPos_);
        flagShader_->setVec3("uSkyZenith", skyZenith);
        flagShader_->setVec3("uSkyHorizon", skyHorizon);
        flagShader_->setFloat("uTime", timeSec);
        flagShader_->setFloat("uDayLight", dn.dayLight);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
        constexpr float kFlagDrawDist = 200.0f;
        for (const voxel::SceneryFlag& f : world_.sceneryFlags()) {
            const glm::vec3 fp(f.poleX, f.clothAnchorY, f.poleZ);
            if (glm::length(fp - camPos_) > kFlagDrawDist)
                continue;
            const glm::mat4 model =
                glm::translate(glm::mat4(1.0f), glm::vec3(f.poleX, f.clothAnchorY, f.poleZ)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(f.yawDeg), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::scale(glm::mat4(1.0f), glm::vec3(2.85f, 2.85f, 1.0f));
            flagShader_->setMat4("uModel", model);
            flagShader_->setFloat("uPhase", f.poleX * 0.31f + f.poleZ * 0.19f);
            flagShader_->setInt("uColorSeed", f.colorSeed);
            scenery_.drawElements();
        }
    }

    if (smokePlumes_.valid() && !world_.smokePuffs().empty()) {
        const int vh = voxel::VoxelWorld::terrainHeight(villageAnchorWx_, villageAnchorWz_);
        const glm::vec3 villageCenter(static_cast<float>(villageAnchorWx_) + 0.5f, static_cast<float>(vh) + 10.0f,
                                      static_cast<float>(villageAnchorWz_) + 0.5f);
        if (glm::length(camPos_ - villageCenter) < 220.0f)
            smokePlumes_.draw(vp, front, timeSec, world_.smokePuffs());
    }

    const bool wantRefr = world_.hasAnyGlassMesh() && refrFbo_ && refrColorTex_;
    if (wantRefr) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, refrFbo_);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glBlitFramebuffer(0, 0, fbW_, fbH_, 0, 0, refrW_, refrH_, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    chunkShader_->use();
    chunkShader_->setInt("uGlassOnly", 1);
    chunkShader_->setFloat("uDayLight", dn.dayLight);
    syncLightingFromScene(*chunkShader_, engine_.lighting(), world_, camPos_, dn);
    chunkShader_->setInt("uHasSceneRefr", wantRefr ? 1 : 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, voxelAtlas_.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTex_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, voxelAtlas_.normalTexture);
    chunkShader_->setInt("uHasAtlasN", voxelAtlas_.normalTexture ? 1 : 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, wantRefr ? refrColorTex_ : 0);
    for (const auto& kv : world_.chunks()) {
        const voxel::Chunk* ch = kv.second.get();
        if (ch->hasGlass() && chunkInDrawRange(ch, camPos_))
            ch->drawGlass();
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbW_, fbH_);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, fbW_, fbH_, 0, 0, fbW_, fbH_, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const float sea = voxel::kSeaLevel;
    waterShader_->use();
    waterShader_->setMat4("uMvp", vp);
    waterShader_->setVec3("viewPos", camPos_);
    waterShader_->setVec3("uSunDir", dn.shadeSunDir);
    waterShader_->setFloat("uTime", timeSec);
    waterShader_->setFloat("uSeaLevel", sea);
    waterShader_->setVec3("uSkyHorizon", skyHorizon);
    waterShader_->setVec3("uSkyZenith", skyZenith);
    waterShader_->setFloat("uDayLight", dn.dayLight);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneColorTex_);
    waterShader_->setInt("sceneColor", 0);

    for (const auto& kv : world_.chunks()) {
        const voxel::Chunk* ch = kv.second.get();
        if (ch->hasWater() && chunkInDrawRange(ch, camPos_))
            ch->drawWater();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    if (mouseCaptured_) {
        if (aimValid_ && blockOutlineVao_ != 0 && hudShader_) {
            const float aspect = fbH_ > 0 ? static_cast<float>(fbW_) / static_cast<float>(fbH_) : 1.0f;
            const glm::mat4 proj = glm::perspective(glm::radians(64.0f), aspect, 0.1f, 320.0f);
            glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
            const glm::mat4 view = glm::lookAt(camPos_, camPos_ + front, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(aimBlock_)) *
                                    glm::scale(glm::mat4(1.0f), glm::vec3(1.002f));
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            hudShader_->use();
            hudShader_->setMat4("mvp", proj * view * model);
            const float hi = 0.55f + 0.25f * std::sin(timeSec * 6.0f);
            hudShader_->setVec3("color", glm::vec3(0.35f, 0.95f, hi));
            hudShader_->setFloat("uAlpha", 0.92f);
            glLineWidth(1.8f);
            glBindVertexArray(blockOutlineVao_);
            glDrawArrays(GL_LINES, 0, 24);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS);
        }
        drawCrosshair();
        drawHud(timeSec);
    }
}

int VoxelApplication::run() {
    if (!init())
        return 1;
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    while (!window_.shouldClose()) {
        window_.pollEvents();
        const auto t1 = clock::now();
        const float dt = std::chrono::duration<float>(t1 - t0).count();
        t0 = t1;
        const float timeSec = std::chrono::duration<float>(t1.time_since_epoch()).count();
        update(dt);
        render(timeSec);
        window_.swapBuffers();
    }
    shutdown();
    return 0;
}
