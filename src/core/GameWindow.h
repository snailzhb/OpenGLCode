#pragma once

struct GLFWwindow;

/** GLFW 窗口与 OpenGL 上下文（core 层：仅负责创建与交换，不含游戏逻辑） */
class GameWindow {
public:
    bool create(int width, int height, const char* title);
    void destroy();

    GLFWwindow* native() const { return window_; }
    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();
    void framebufferSize(int& outW, int& outH) const;

private:
    GLFWwindow* window_ = nullptr;
};
