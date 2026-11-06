#pragma once

#include "core/GameWindow.h"
#include "engine/Engine.h"
#include "engine/physics/CharacterController.h"
#include "engine/physics/VoxelCollider.h"
#include "game/GameAPI.h"
#include "rendering/RigidMesh.h"
#include "rendering/ScaledBlockRenderer.h"
#include "rendering/SceneryRenderer.h"
#include "rendering/SmokePlumeRenderer.h"
#include "rendering/VoxelAtlas.h"
#include "voxel/VoxelWorld.h"

#include <learnopengl/shader.h>

#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <string>

/** 体素世界主循环：地形生成、水面折射、水下雾效、放置/破坏方块 */
struct GLFWwindow;
class VoxelApplication {
    friend void VoxelApp_framebufferSize(GLFWwindow* w, int width, int height);
    friend void VoxelApp_key(GLFWwindow* w, int key, int scancode, int action, int mods);
    friend void VoxelApp_cursor(GLFWwindow* w, double x, double y);
    friend void VoxelApp_mouseButton(GLFWwindow* w, int button, int action, int mods);
    friend void VoxelApp_scroll(GLFWwindow* w, double xoff, double yoff);

public:
    int run();

private:
    bool init();
    void shutdown();
    void onFramebufferSize(int w, int h);
    void update(float dt);
    void render(float timeSec);

    void recreateSceneFbo(int w, int h);
    void destroySceneFbo();
    void destroyRefrFbo();
    void recreateRefrFbo();
    void createShadowMap();
    void destroyShadowMap();
    void initHudGeometry();
    void destroyHudGeometry();
    void drawCrosshair();
    void drawHud(float timeSec);
    void drawSky(const glm::mat4& proj, const glm::mat4& view, float timeSec, const glm::vec3& sunSkyWorld,
                  const glm::vec3& horizon, const glm::vec3& zenith, float dayLight);
    void toggleFullscreen();

    bool intersectsPlayer(const glm::ivec3& block) const;
    void updateAimTarget();
    void updatePlayerMovement(float dt);
    void tryInteract();
    void tryPickBlock();
    void updateExploration(float dt);
    void setHotbarIndex(int idx);
    void pushToast(const std::string& msg, float durationSec = 3.5f);

    GameWindow window_;

    std::unique_ptr<Shader> chunkShader_;
    std::unique_ptr<Shader> depthShader_;
    std::unique_ptr<Shader> waterShader_;
    std::unique_ptr<Shader> hudShader_;
    std::unique_ptr<Shader> skyShader_;
    std::unique_ptr<Shader> flagShader_;
    std::unique_ptr<Shader> flagDepthShader_;

    RigidMesh rigidSky_;
    VoxelAtlas voxelAtlas_;
    SceneryRenderer scenery_;
    SmokePlumeRenderer smokePlumes_;
    ScaledBlockRenderer scaledBlocks_;

    engine::Engine engine_;
    engine::physics::CharacterController player_;
    std::unique_ptr<engine::physics::VoxelCollider> collider_;

    voxel::VoxelWorld world_;

    int villageAnchorWx_ = 24;
    int villageAnchorWz_ = 22;

    glm::vec3 camPos_{0.0f, 52.0f, 8.0f};
    float yawDeg_ = -90.0f;
    float pitchDeg_ = 0.0f;
    float mouseSens_ = 0.12f;
    bool mouseCaptured_ = true;
    bool firstMouse_ = true;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;

    bool keyW_ = false, keyA_ = false, keyS_ = false, keyD_ = false;
    bool keySpace_ = false, keyShift_ = false;
    bool mouseLPrev_ = false, mouseRPrev_ = false;

    static constexpr int kHotbarSlots_ = 10;
    std::array<voxel::BlockId, kHotbarSlots_> hotbar_{{
        voxel::BlockId::Grass,
        voxel::BlockId::Dirt,
        voxel::BlockId::Stone,
        voxel::BlockId::Sand,
        voxel::BlockId::Wood,
        voxel::BlockId::Cobble,
        voxel::BlockId::Glass,
        voxel::BlockId::GlassLight,
        voxel::BlockId::GlassDark,
        voxel::BlockId::LampWarm,
    }};
    int hotbarIndex_ = 5;
    voxel::BlockId placeBlock_ = voxel::BlockId::Cobble;

    /** 行走模式：重力+碰撞；F 切换为自由飞行 */
    bool walkMode_ = true;

    bool aimValid_ = false;
    glm::ivec3 aimBlock_{0};
    glm::ivec3 aimNormal_{0};
    float aimDist_ = 0.0f;

    bool interactHint_ = false;
    std::string toastMsg_;
    float toastRemainSec_ = 0.0f;

    static constexpr int kLandmarkCount_ = 4;
    std::array<glm::vec3, kLandmarkCount_> landmarkPos_{};
    std::array<const char*, kLandmarkCount_> landmarkName_{};
    std::array<bool, kLandmarkCount_> landmarkFound_{};
    int exploreScore_ = 0;

    int fbW_ = voxel::kWindowW;
    int fbH_ = voxel::kWindowH;
    unsigned int sceneFbo_ = 0;
    unsigned int sceneColorTex_ = 0;
    unsigned int sceneDepthRbo_ = 0;

    /** 半分辨率：不透明+旗帜后的颜色，供玻璃折射采样 */
    unsigned int refrFbo_ = 0;
    unsigned int refrColorTex_ = 0;
    int refrW_ = 0;
    int refrH_ = 0;

    unsigned int shadowFbo_ = 0;
    unsigned int shadowDepthTex_ = 0;
    static constexpr int kShadowMapSize_ = 1536;

    unsigned int crosshairVao_ = 0;
    unsigned int crosshairVbo_ = 0;
    unsigned int hudQuadVao_ = 0;
    unsigned int hudQuadVbo_ = 0;
    unsigned int blockOutlineVao_ = 0;
    unsigned int blockOutlineVbo_ = 0;

    int winRestoreX_ = 80;
    int winRestoreY_ = 80;
    int winRestoreW_ = voxel::kWindowW;
    int winRestoreH_ = voxel::kWindowH;

    /** GLFW 3.0 无 glfwSetWindowMonitor：用铺满主屏的窗口模拟全屏 */
    bool borderlessMaximized_ = false;

    static constexpr int kStreamHalf_ = 4;
    /** 仅当相机跨过 chunk 边界时再 stream，减轻每帧 CPU 压力 */
    bool streamPrimed_ = false;
    int streamCx_ = 0;
    int streamCz_ = 0;
    float titleIntervalAccum_ = 0.0f;
    /** 开局几秒略提高 chunk 创建预算，避免出生点周围装饰“慢慢长” */
    float streamWarmupRemainSec_ = 0.0f;
};
