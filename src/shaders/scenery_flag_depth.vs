#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aBend;

uniform mat4 uLightMvp;
uniform mat4 uModel;
uniform float uTime;
uniform float uPhase;

void main() {
    float bend = clamp(aBend, 0.0, 1.0);
    float wobble = sin(uTime * 2.6 + uPhase + aPos.x * 7.5) * 0.07;
    float ripple = sin(uTime * 6.1 + aPos.y * 9.0 + uPhase * 0.3) * 0.032;
    vec3 lp = aPos;
    lp.z += (wobble + ripple) * bend;
    vec4 world = uModel * vec4(lp, 1.0);
    gl_Position = uLightMvp * world;
}
