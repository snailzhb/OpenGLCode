#include "core/GameWindow.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

bool GameWindow::create(int width, int height, const char* title) {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        std::cerr << "GameWindow: glfwCreateWindow failed\n";
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window_);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "GameWindow: gladLoadGLLoader failed\n";
        destroy();
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    return true;
}

void GameWindow::destroy() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool GameWindow::shouldClose() const {
    return window_ && glfwWindowShouldClose(window_);
}

void GameWindow::pollEvents() {
    glfwPollEvents();
}

void GameWindow::swapBuffers() {
    if (window_)
        glfwSwapBuffers(window_);
}

void GameWindow::framebufferSize(int& outW, int& outH) const {
    if (window_)
        glfwGetFramebufferSize(window_, &outW, &outH);
    else {
        outW = outH = 0;
    }
}
