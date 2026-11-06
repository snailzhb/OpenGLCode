#pragma once

#include "core/GameWindow.h"
#include "engine/Engine.h"
#include "engine/physics/CharacterController.h"
#include "engine/physics/MeshTerrainCollider.h"
#include "game/meshcraft/MeshCraftTypes.h"
#include "game/meshcraft/MeshCraftWorld.h"
#include "rendering/MeshSceneRenderer.h"
#include "rendering/RigidMesh.h"

#include <learnopengl/shader.h>

#include <glm/glm.hpp>
#include <memory>
#include <string>

/** MeshCraft：引擎驱动的 mesh 地形 + 网格建造游戏 */
struct GLFWwindow;
class MeshCraftApplication {
    friend void MeshCraft_framebufferSize(GLFWwindow* w, int width, int height);
    friend void MeshCraft_key(GLFWwindow* w, int key, int scancode, int action, int mods);
    friend void MeshCraft_cursor(GLFWwindow* w, double x, double y);
    friend void MeshCraft_scroll(GLFWwindow* w, double xoff, double yoff);
    friend void MeshCraft_mouseButton(GLFWwindow* w, int button, int action, int mods);

public:
    int run();

private:
    bool init();
    void shutdown();
    void onFramebufferSize(int w, int h);
    void update(float dt);
    void render(float timeSec);

    void createShadowMap();
    void destroyShadowMap();
    void drawSky(float timeSec, const glm::mat4& proj, const glm::mat4& view);
    void drawHud();
    void drawHotbar(float timeSec);
    void setHotbarIndex(int idx);
    void onWorldClick(bool place);
    void syncSceneLights(const glm::vec3& cam, float dayLight, const glm::vec3& skyZenith,
                         const glm::vec3& skyHorizon, const glm::vec3& shadeSunDir);

    GameWindow window_;
    engine::Engine engine_;
    engine::physics::CharacterController player_;
    std::unique_ptr<engine::physics::MeshTerrainCollider> collider_;

    meshcraft::MeshCraftWorld world_;
    MeshSceneRenderer scene_;
    RigidMesh skyMesh_;

    std::unique_ptr<Shader> skyShader_;
    std::unique_ptr<Shader> hudShader_;

    glm::vec3 camPos_{0.0f, 40.0f, 80.0f};
    float yawDeg_ = -120.0f;
    float pitchDeg_ = -18.0f;
    float mouseSens_ = 0.12f;
    bool mouseCaptured_ = true;
    bool firstMouse_ = true;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0f;

    bool keyW_ = false, keyA_ = false, keyS_ = false, keyD_ = false;
    bool keySpace_ = false, keyShift_ = false;
    bool walkMode_ = true;

    meshcraft::BuildingKind placeKind_ = meshcraft::BuildingKind::Wall;
    int hotbarIndex_ = 1;

    int fbW_ = 1280;
    int fbH_ = 720;
    unsigned int shadowFbo_ = 0;
    unsigned int shadowDepthTex_ = 0;
    static constexpr int kShadowMapSize_ = 1536;

    unsigned int hudVao_ = 0;
    unsigned int hudVbo_ = 0;
    unsigned int hudQuadVao_ = 0;
    unsigned int hudQuadVbo_ = 0;
    static constexpr int kHotbarSlots_ = 6;
    std::string toastMsg_;
    float toastRemainSec_ = 0.0f;
};
