#pragma once

#include <glm/glm.hpp>

/** 程序化旗帜网格：顶点着色器风力变形，供体素场景叠加绘制 */
class SceneryRenderer {
public:
    void init();
    void shutdown();

    bool valid() const { return vao_ != 0; }
    int indexCount() const { return indexCount_; }

    void drawElements() const;

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int indexCount_ = 0;
};
