#pragma once

#include <glad/glad.h>

/** 单位立方体 + 单位四边形 + 单位球（用于天空体、水面、抛体等） */
class RigidMesh {
public:
    void initUnitCube();
    void initUnitQuad();
    void initUnitSphere();
    void shutdown();

    void drawCube() const;
    void drawLampCube() const;
    void drawQuad() const;
    void drawSphere() const;

    unsigned int unitCubeVao() const { return cubeVao_; }
    unsigned int unitCubeVbo() const { return cubeVbo_; }

private:
    unsigned int cubeVao_ = 0;
    unsigned int cubeVbo_ = 0;
    unsigned int lampVao_ = 0;
    unsigned int quadVao_ = 0;
    unsigned int quadVbo_ = 0;
    unsigned int sphereVao_ = 0;
    unsigned int sphereVbo_ = 0;
    unsigned int sphereEbo_ = 0;
    int sphereIndexCount_ = 0;
};
