#include "rendering/SmokePlumeRenderer.h"

#include "voxel/VoxelWorld.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

GLuint compile(GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, 512, nullptr, buf);
        std::cerr << "SmokePlume shader compile failed:\n" << buf << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint link(GLuint vs, GLuint fs) {
    const GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetProgramInfoLog(p, 512, nullptr, buf);
        std::cerr << "SmokePlume program link failed:\n" << buf << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

const char* kVs = R"GLSL(
#version 330 core
layout(location = 0) in float aVid;
layout(location = 1) in vec4 iPosScale;
layout(location = 2) in vec4 iTintPhase;

uniform mat4 uMvp;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform float uTime;

out vec2 vCr;
out float vAlphaBase;
out vec3 vCol;
out float vPhase;

const vec2 kCorners[6] = vec2[](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

void main() {
    vec3 c = iPosScale.xyz;
    float s = iPosScale.w;
    int vid = int(aVid + 0.5);
    vec2 cr = kCorners[vid];
    float ph = iTintPhase.w;
    float age = clamp((s - 0.18) * 3.2, 0.0, 1.0);
    vec3 wind = vec3(sin(uTime * 0.62 + ph), 0.0, cos(uTime * 0.48 + ph * 1.07)) * age * 0.42;
    wind += vec3(sin(uTime * 1.15 + ph * 2.0) * 0.14, sin(uTime * 0.9 + ph) * 0.06, cos(uTime * 0.88 + ph * 1.4) * 0.11) * age;
    float bob = sin(uTime * 1.1 + ph) * 0.06 + cos(uTime * 0.73 + ph * 0.5) * 0.04;
    vec3 world = c + uCamRight * (cr.x * s * 2.2) + uCamUp * (cr.y * s * 2.5 + bob) + wind;
    gl_Position = uMvp * vec4(world, 1.0);
    vCr = cr;
    vPhase = ph;
    vAlphaBase = clamp(0.14 + 0.58 * (1.0 - age * 0.35), 0.0, 0.82) * (0.52 + 0.48 * sin(ph + uTime * 0.85));
    vCol = iTintPhase.rgb;
}
)GLSL";

const char* kFs = R"GLSL(
#version 330 core
in vec2 vCr;
in float vAlphaBase;
in vec3 vCol;
in float vPhase;
out vec4 FragColor;

uniform float uTime;

void main() {
    vec2 uv = vCr * vec2(0.92, 1.06);
    float r2 = dot(uv, uv);
    float puff = exp(-r2 * 2.75);
    float len = length(vCr);
    float edgeFade = smoothstep(1.12, 0.42, len);
    float rip = sin(dot(vCr, vec2(7.1, 5.3)) * 6.2 + uTime * 1.65 + vPhase) * 0.5 + 0.5;
    float a = vAlphaBase * puff * (0.5 + 0.5 * rip) * edgeFade;
    vec3 rgb = vCol * (0.86 + 0.14 * rip);
    FragColor = vec4(rgb, a);
}
)GLSL";

} // namespace

void SmokePlumeRenderer::init() {
    shutdown();
    const GLuint vs = compile(GL_VERTEX_SHADER, kVs);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, kFs);
    if (!vs || !fs) {
        if (vs)
            glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        return;
    }
    shader_ = link(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!shader_)
        return;

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &quadVbo_);
    glGenBuffers(1, &instanceVbo_);

    float ids[6] = {0, 1, 2, 3, 4, 5};
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ids), ids, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 1, GL_FLOAT, GL_FALSE, sizeof(float), nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * 512, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 2, reinterpret_cast<void*>(sizeof(glm::vec4)));
    glVertexAttribDivisor(2, 1);
    instanceCap_ = 512;

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SmokePlumeRenderer::shutdown() {
    if (quadVbo_) {
        glDeleteBuffers(1, &quadVbo_);
        quadVbo_ = 0;
    }
    if (instanceVbo_) {
        glDeleteBuffers(1, &instanceVbo_);
        instanceVbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (shader_) {
        glDeleteProgram(shader_);
        shader_ = 0;
    }
    instanceCap_ = 0;
}

void SmokePlumeRenderer::draw(const glm::mat4& vp, const glm::vec3& camForward, float timeSec,
                              const std::vector<voxel::ScenerySmokePuff>& puffs) const {
    if (!valid() || !shader_ || puffs.empty())
        return;

    const size_t n = puffs.size();
    if (n > instanceCap_) {
        instanceCap_ = std::max(instanceCap_ * 2, n);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * 2 * instanceCap_, nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    std::vector<glm::vec4> interleaved;
    interleaved.resize(n * 2);
    for (size_t i = 0; i < n; ++i) {
        interleaved[i * 2] = puffs[i].posScale;
        interleaved[i * 2 + 1] = puffs[i].tintPhase;
    }

    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(interleaved.size() * sizeof(glm::vec4)), interleaved.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(camForward, worldUp);
    if (glm::dot(right, right) < 1e-8f)
        right = glm::cross(camForward, glm::vec3(1.0f, 0.0f, 0.0f));
    right = glm::normalize(right);
    const glm::vec3 up = glm::normalize(glm::cross(right, camForward));

    glUseProgram(shader_);
    glUniformMatrix4fv(glGetUniformLocation(shader_, "uMvp"), 1, GL_FALSE, glm::value_ptr(vp));
    glUniform3fv(glGetUniformLocation(shader_, "uCamRight"), 1, glm::value_ptr(right));
    glUniform3fv(glGetUniformLocation(shader_, "uCamUp"), 1, glm::value_ptr(up));
    glUniform1f(glGetUniformLocation(shader_, "uTime"), timeSec);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(n));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glUseProgram(0);
}
