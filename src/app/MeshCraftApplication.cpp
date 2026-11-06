#include "app/MeshCraftApplication.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace {

struct DayNight {
    glm::vec3 visualSunDir{};
    glm::vec3 shadeSunDir{};
    glm::vec3 skyHorizon{};
    glm::vec3 skyZenith{};
    float dayLight = 1.0f;
};

DayNight computeDayNight(float timeSec) {
    constexpr float period = 240.0f;
    const float a = timeSec * (6.2831853f / period);
    const glm::vec3 sunFromVisual =
        glm::normalize(glm::vec3(std::cos(a) * 0.58f, 0.17f + 0.83f * std::sin(a), std::sin(a * 0.74f) * 0.42f));
    const float el = sunFromVisual.y;
    float dayLight = glm::clamp((el + 0.18f) / 1.02f, 0.0f, 1.0f);
    dayLight = glm::max(dayLight, 0.08f);
    const glm::vec3 moonFrom = glm::normalize(glm::vec3(0.48f, 0.62f, 0.36f));
    const float w = glm::smoothstep(-0.18f, 0.22f, el);
    const glm::vec3 sunFromShade = glm::normalize(glm::mix(moonFrom, sunFromVisual, w));
    DayNight d;
    d.visualSunDir = -sunFromVisual;
    d.shadeSunDir = -sunFromShade;
    d.skyHorizon = glm::mix(glm::vec3(0.04f, 0.05f, 0.12f), glm::vec3(0.62f, 0.80f, 1.0f), glm::pow(dayLight, 0.82f));
    d.skyZenith = glm::mix(glm::vec3(0.005f, 0.015f, 0.06f), glm::vec3(0.22f, 0.52f, 0.98f), glm::pow(dayLight, 0.82f));
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

const char* kindName(meshcraft::BuildingKind k) {
    switch (k) {
    case meshcraft::BuildingKind::Foundation:
        return "Foundation";
    case meshcraft::BuildingKind::Wall:
        return "Wall";
    case meshcraft::BuildingKind::WallWindow:
        return "Window Wall";
    case meshcraft::BuildingKind::Pillar:
        return "Pillar";
    case meshcraft::BuildingKind::Roof:
        return "Roof";
    case meshcraft::BuildingKind::Stairs:
        return "Stairs";
    default:
        return "?";
    }
}

meshcraft::BuildingKind hotbarKind(int idx) {
    static const meshcraft::BuildingKind tbl[] = {
        meshcraft::BuildingKind::Foundation, meshcraft::BuildingKind::Wall, meshcraft::BuildingKind::WallWindow,
        meshcraft::BuildingKind::Pillar, meshcraft::BuildingKind::Roof, meshcraft::BuildingKind::Stairs};
    if (idx < 0 || idx >= 6)
        return meshcraft::BuildingKind::Wall;
    return tbl[idx];
}

glm::vec3 kindHudColor(meshcraft::BuildingKind k) {
    switch (k) {
    case meshcraft::BuildingKind::Foundation:
        return glm::vec3(0.48f, 0.32f, 0.16f);
    case meshcraft::BuildingKind::Wall:
        return glm::vec3(0.55f, 0.54f, 0.52f);
    case meshcraft::BuildingKind::WallWindow:
        return glm::vec3(0.55f, 0.75f, 0.88f);
    case meshcraft::BuildingKind::Pillar:
        return glm::vec3(0.42f, 0.40f, 0.38f);
    case meshcraft::BuildingKind::Roof:
        return glm::vec3(0.62f, 0.22f, 0.14f);
    case meshcraft::BuildingKind::Stairs:
        return glm::vec3(0.50f, 0.46f, 0.40f);
    default:
        return glm::vec3(0.5f);
    }
}

glm::vec3 horizontalMoveDir(const glm::vec3& front) {
    glm::vec3 h(front.x, 0.0f, front.z);
    if (glm::dot(h, h) < 1e-8f)
        return glm::vec3(0.0f, 0.0f, -1.0f);
    return glm::normalize(h);
}

#ifndef SHADER_ROOT
#define SHADER_ROOT "shaders"
#endif

} // namespace

void MeshCraft_framebufferSize(GLFWwindow* w, int width, int height) {
    if (auto* a = static_cast<MeshCraftApplication*>(glfwGetWindowUserPointer(w)))
        a->onFramebufferSize(width, height);
}

void MeshCraft_key(GLFWwindow* w, int key, int, int action, int) {
    auto* a = static_cast<MeshCraftApplication*>(glfwGetWindowUserPointer(w));
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
        if (key == GLFW_KEY_TAB) {
            a->mouseCaptured_ = !a->mouseCaptured_;
            glfwSetInputMode(w, GLFW_CURSOR, a->mouseCaptured_ ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            if (a->mouseCaptured_)
                a->firstMouse_ = true;
        }
        if (key == GLFW_KEY_F)
            a->walkMode_ = !a->walkMode_;
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6)
            a->setHotbarIndex(key - GLFW_KEY_1);
    }
}

void MeshCraft_cursor(GLFWwindow* w, double x, double y) {
    auto* a = static_cast<MeshCraftApplication*>(glfwGetWindowUserPointer(w));
    if (!a || !a->mouseCaptured_)
        return;
    if (a->firstMouse_) {
        a->lastMouseX_ = x;
        a->lastMouseY_ = y;
        a->firstMouse_ = false;
        return;
    }
    a->yawDeg_ += static_cast<float>(x - a->lastMouseX_) * a->mouseSens_;
    a->pitchDeg_ += static_cast<float>(a->lastMouseY_ - y) * a->mouseSens_;
    a->lastMouseX_ = x;
    a->lastMouseY_ = y;
    a->pitchDeg_ = glm::clamp(a->pitchDeg_, -89.0f, 89.0f);
}

void MeshCraft_mouseButton(GLFWwindow* w, int button, int action, int) {
    auto* a = static_cast<MeshCraftApplication*>(glfwGetWindowUserPointer(w));
    if (!a || !a->mouseCaptured_ || action != GLFW_PRESS)
        return;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        a->onWorldClick(false);
    if (button == GLFW_MOUSE_BUTTON_RIGHT)
        a->onWorldClick(true);
}

void MeshCraft_scroll(GLFWwindow* w, double, double yoff) {
    auto* a = static_cast<MeshCraftApplication*>(glfwGetWindowUserPointer(w));
    if (!a || !a->mouseCaptured_)
        return;
    if (yoff > 0)
        a->setHotbarIndex(a->hotbarIndex_ - 1);
    else if (yoff < 0)
        a->setHotbarIndex(a->hotbarIndex_ + 1);
}

void MeshCraftApplication::onFramebufferSize(int w, int h) {
    if (w <= 0 || h <= 0)
        return;
    fbW_ = w;
    fbH_ = h;
    glViewport(0, 0, w, h);
}

void MeshCraftApplication::createShadowMap() {
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
    const float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void MeshCraftApplication::destroyShadowMap() {
    if (shadowDepthTex_) {
        glDeleteTextures(1, &shadowDepthTex_);
        shadowDepthTex_ = 0;
    }
    if (shadowFbo_) {
        glDeleteFramebuffers(1, &shadowFbo_);
        shadowFbo_ = 0;
    }
}

void MeshCraftApplication::syncSceneLights(const glm::vec3& cam, float dayLight, const glm::vec3& skyZenith,
                                             const glm::vec3& skyHorizon, const glm::vec3& shadeSunDir) {
    auto& lighting = engine_.lighting();
    engine::lighting::DirectionalLight sun;
    sun.direction = shadeSunDir;
    sun.color = glm::vec3(1.14f, 1.02f, 0.86f);
    sun.intensity = glm::mix(0.45f, 1.15f, dayLight);
    lighting.setDirectional(sun);
    engine::lighting::AmbientSettings amb;
    amb.skyZenith = skyZenith;
    amb.skyHorizon = skyHorizon;
    amb.dayLight = dayLight;
    amb.hemisphereStrength = 0.82f;
    lighting.setAmbient(amb);

    lighting.clearDynamicPointLights();
    const glm::ivec2 hg = world_.hamletGrid();
    const float hwx = (static_cast<float>(hg.x) + 0.5f) * meshcraft::MeshBuildingWorld::kGrid;
    const float hwz = (static_cast<float>(hg.y) + 0.5f) * meshcraft::MeshBuildingWorld::kGrid;
    const glm::vec3 hamlet(hwx, world_.terrain().sampleHeight(hwx, hwz) + 2.0f, hwz);
    lighting.addFlickerTorch(hamlet, glm::vec3(1.0f, 0.55f, 0.22f), 2.2f, 20.0f, 0.0f);
    lighting.addFlickerTorch(hamlet + glm::vec3(14.0f, 0.0f, 6.0f), glm::vec3(1.0f, 0.7f, 0.35f), 1.5f, 16.0f, 1.0f);
    lighting.addFlickerTorch(hamlet + glm::vec3(-12.0f, 0.0f, -8.0f), glm::vec3(0.45f, 0.85f, 1.0f), 1.3f, 14.0f, 2.0f);
    lighting.buildDynamicPointLights(cam);
}

bool MeshCraftApplication::init() {
    if (!window_.create(1280, 720, "MeshCraft — GameEigen Engine")) {
        std::cerr << "MeshCraftApplication: window failed\n";
        return false;
    }
    GLFWwindow* w = window_.native();
    glfwSetWindowUserPointer(w, this);
    glfwSetFramebufferSizeCallback(w, MeshCraft_framebufferSize);
    glfwSetKeyCallback(w, MeshCraft_key);
    glfwSetCursorPosCallback(w, MeshCraft_cursor);
    glfwSetScrollCallback(w, MeshCraft_scroll);
    glfwSetMouseButtonCallback(w, MeshCraft_mouseButton);
    glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    window_.framebufferSize(fbW_, fbH_);
    glViewport(0, 0, fbW_, fbH_);

    if (!engine_.init()) {
        std::cerr << "MeshCraftApplication: engine init failed\n";
        return false;
    }

    if (!world_.build(256, 2026)) {
        std::cerr << "MeshCraftApplication: world build failed\n";
        return false;
    }

    collider_ = std::make_unique<engine::physics::MeshTerrainCollider>(world_.terrain(), world_.buildings());

    scene_.init();
    skyMesh_.initUnitSphere();

    const std::string sd = std::string(SHADER_ROOT) + "/";
    try {
        skyShader_ = std::make_unique<Shader>((sd + "sky_gradient.vs").c_str(), (sd + "sky_gradient.fs").c_str());
        hudShader_ = std::make_unique<Shader>((sd + "unlit_color.vs").c_str(), (sd + "unlit_color.fs").c_str());
    } catch (...) {
        std::cerr << "MeshCraftApplication: shader load failed\n";
        return false;
    }

    const float cross[] = {
        -0.05f, 0, 0, -0.012f, 0, 0, 0.012f, 0, 0, 0.05f, 0, 0,
        0, -0.05f, 0, 0, -0.012f, 0, 0, 0.012f, 0, 0, 0.05f, 0,
    };
    glGenVertexArrays(1, &hudVao_);
    glGenBuffers(1, &hudVbo_);
    glBindVertexArray(hudVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cross), cross, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &hudQuadVao_);
    glGenBuffers(1, &hudQuadVbo_);
    glBindVertexArray(hudQuadVao_);
    glBindBuffer(GL_ARRAY_BUFFER, hudQuadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    createShadowMap();
    setHotbarIndex(hotbarIndex_);

    camPos_ = world_.spawnEyePosition();
    player_.setEyePosition(camPos_);
    yawDeg_ = -150.0f;
    pitchDeg_ = -12.0f;

    toastMsg_ = "MeshCraft: LMB remove, RMB place, 1-6 parts, F fly, Tab mouse";
    toastRemainSec_ = 6.0f;
    return scene_.valid();
}

void MeshCraftApplication::shutdown() {
    if (hudVbo_) {
        glDeleteBuffers(1, &hudVbo_);
        hudVbo_ = 0;
    }
    if (hudVao_) {
        glDeleteVertexArrays(1, &hudVao_);
        hudVao_ = 0;
    }
    if (hudQuadVbo_) {
        glDeleteBuffers(1, &hudQuadVbo_);
        hudQuadVbo_ = 0;
    }
    if (hudQuadVao_) {
        glDeleteVertexArrays(1, &hudQuadVao_);
        hudQuadVao_ = 0;
    }
    destroyShadowMap();
    scene_.shutdown();
    skyMesh_.shutdown();
    skyShader_.reset();
    hudShader_.reset();
    collider_.reset();
    world_.shutdownGpu();
    engine_.shutdown();
    window_.destroy();
}

void MeshCraftApplication::setHotbarIndex(int idx) {
    hotbarIndex_ = (idx % kHotbarSlots_ + kHotbarSlots_) % kHotbarSlots_;
    placeKind_ = hotbarKind(hotbarIndex_);
}

void MeshCraftApplication::onWorldClick(bool place) {
    if (!mouseCaptured_ || !collider_)
        return;

    const glm::vec3 rayDir = eulerToForward(yawDeg_, pitchDeg_);
    if (place) {
        if (world_.tryPlace(placeKind_, camPos_, rayDir)) {
            toastMsg_ = std::string("Placed ") + kindName(placeKind_);
        } else {
            toastMsg_ = "Cannot place here";
        }
    } else {
        if (world_.tryRemove(camPos_, rayDir))
            toastMsg_ = "Removed structure";
        else
            toastMsg_ = "Nothing to remove";
    }
    toastRemainSec_ = 2.0f;
}

void MeshCraftApplication::update(float dt) {
    engine_.beginFrame(dt);
    if (toastRemainSec_ > 0.0f)
        toastRemainSec_ = std::max(0.0f, toastRemainSec_ - dt);

    if (!collider_)
        return;

    const glm::vec3 look = eulerToForward(yawDeg_, pitchDeg_);
    const glm::vec3 front = walkMode_ ? horizontalMoveDir(look) : look;
    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
    glm::vec3 move(0);
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

void MeshCraftApplication::drawSky(float timeSec, const glm::mat4& proj, const glm::mat4& view) {
    if (!skyShader_)
        return;
    const DayNight dn = computeDayNight(timeSec);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glCullFace(GL_FRONT);
    const glm::mat4 model = glm::translate(glm::mat4(1.0f), camPos_) * glm::scale(glm::mat4(1.0f), glm::vec3(220.0f));
    skyShader_->use();
    skyShader_->setMat4("uProj", proj);
    skyShader_->setMat4("uView", view);
    skyShader_->setMat4("uModel", model);
    skyShader_->setVec3("uCamPos", camPos_);
    skyShader_->setVec3("uHorizon", dn.skyHorizon);
    skyShader_->setVec3("uZenith", dn.skyZenith);
    skyShader_->setVec3("uSunDirWorld", glm::normalize(-dn.visualSunDir));
    skyShader_->setFloat("uDayLight", dn.dayLight);
    skyShader_->setFloat("uSunBloom", 0.72f + 0.07f * std::sin(timeSec * 0.35f));
    skyMesh_.drawSphere();
    glCullFace(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void MeshCraftApplication::drawHud() {
    if (!hudShader_ || hudVao_ == 0)
        return;
    glDisable(GL_DEPTH_TEST);
    hudShader_->use();
    hudShader_->setMat4("mvp", glm::mat4(1.0f));
    hudShader_->setVec3("color", glm::vec3(1.0f, 1.0f, 1.0f));
    hudShader_->setFloat("uAlpha", 1.0f);
    glLineWidth(2.0f);
    glBindVertexArray(hudVao_);
    glDrawArrays(GL_LINES, 0, 8);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void MeshCraftApplication::drawHotbar(float timeSec) {
    if (!hudShader_ || hudQuadVao_ == 0)
        return;
    (void)timeSec;
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

    drawNdcQuad(-0.42f, -0.96f, 0.42f, -0.84f, glm::vec3(0.04f, 0.05f, 0.08f), 0.72f);
    for (int i = 0; i < kHotbarSlots_; ++i) {
        const float x0 = x;
        const float x1 = x + slotW;
        const bool sel = i == hotbarIndex_;
        drawNdcQuad(x0 - 0.003f, y0 - 0.008f, x1 + 0.003f, y1 + 0.008f,
                    sel ? glm::vec3(0.95f, 0.82f, 0.35f) : glm::vec3(0.22f, 0.24f, 0.28f), sel ? 0.95f : 0.55f);
        drawNdcQuad(x0, y0, x1, y1, kindHudColor(hotbarKind(i)), 0.92f);
        x = x1 + gap;
    }

    if (toastRemainSec_ > 0.0f && !toastMsg_.empty()) {
        const float fade = glm::clamp(toastRemainSec_ / 0.6f, 0.35f, 1.0f);
        drawNdcQuad(-0.72f, 0.78f, 0.72f, 0.9f, glm::vec3(0.06f, 0.08f, 0.12f), 0.78f * fade);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void MeshCraftApplication::render(float timeSec) {
    const DayNight dn = computeDayNight(timeSec);
    const float aspect = fbH_ > 0 ? static_cast<float>(fbW_) / static_cast<float>(fbH_) : 1.0f;
    const glm::mat4 proj = glm::perspective(glm::radians(64.0f), aspect, 0.1f, 480.0f);
    const glm::vec3 front = eulerToForward(yawDeg_, pitchDeg_);
    const glm::mat4 view = glm::lookAt(camPos_, camPos_ + front, glm::vec3(0, 1, 0));
    const glm::mat4 vp = proj * view;

    syncSceneLights(camPos_, dn.dayLight, dn.skyZenith, dn.skyHorizon, dn.shadeSunDir);

    const glm::vec3 toSun = glm::normalize(-dn.shadeSunDir);
    const glm::vec3 shadowCenter = camPos_;
    const glm::vec3 shadowEye = shadowCenter + toSun * 128.0f;
    glm::mat4 lightView = glm::lookAt(shadowEye, shadowCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    const float orthoExtent = 102.0f;
    const glm::mat4 lightProj = glm::ortho(-orthoExtent, orthoExtent, -orthoExtent, orthoExtent, 0.5f, 280.0f);
    glm::mat4 lightSpace = lightProj * lightView;
    {
        const float texWorld = (2.0f * orthoExtent) / static_cast<float>(kShadowMapSize_);
        glm::vec4 cLight = lightView * glm::vec4(shadowCenter, 1.0f);
        cLight.x = std::floor(cLight.x / texWorld) * texWorld;
        cLight.y = std::floor(cLight.y / texWorld) * texWorld;
        const glm::mat4 invLv = glm::inverse(lightView);
        const glm::vec3 snapped = glm::vec3(invLv * cLight);
        lightView = glm::lookAt(shadowEye, snapped, glm::vec3(0.0f, 1.0f, 0.0f));
        lightSpace = lightProj * lightView;
    }

    glViewport(0, 0, kShadowMapSize_, kShadowMapSize_);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo_);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glCullFace(GL_FRONT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.15f, 850.0f);
    scene_.drawShadowTerrain(world_.terrain(), lightSpace);
    scene_.drawShadowBuildings(world_.buildings(), lightSpace);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, fbW_, fbH_);
    glClearColor(dn.skyHorizon.r * 0.9f, dn.skyHorizon.g * 0.9f, dn.skyHorizon.b * 0.95f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSky(timeSec, proj, view);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    if (scene_.valid()) {
        const float invSh = 1.0f / static_cast<float>(kShadowMapSize_);
        glDisable(GL_CULL_FACE);
        scene_.drawTerrain(world_.terrain(), vp, lightSpace, camPos_, shadowDepthTex_, invSh, engine_.lighting());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        scene_.drawWater(world_.terrain(), vp, timeSec, camPos_, dn.shadeSunDir,
                         glm::vec3(1.14f, 1.02f, 0.86f) * glm::mix(0.72f, 1.15f, dn.dayLight), dn.skyHorizon,
                         dn.skyZenith, dn.dayLight);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        scene_.drawBuildings(world_.buildings(), vp, lightSpace, camPos_, shadowDepthTex_, invSh, engine_.lighting());
        glEnable(GL_CULL_FACE);
    }

    if (mouseCaptured_) {
        drawHud();
        drawHotbar(timeSec);
    }

    GLFWwindow* w = window_.native();
    std::string title = "MeshCraft | ";
    title += kindName(placeKind_);
    title += walkMode_ ? " | Walk" : " | Fly";
    if (toastRemainSec_ > 0.0f && !toastMsg_.empty()) {
        title += " | ";
        title += toastMsg_;
    }
    glfwSetWindowTitle(w, title.c_str());
}

int MeshCraftApplication::run() {
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
