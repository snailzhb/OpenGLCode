#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aBend;

uniform mat4 uMvp;
uniform mat4 uLightSpaceMatrix;
uniform mat4 uModel;
uniform float uTime;
uniform float uPhase;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec4 vFragPosLightSpace;

void main() {
    float bend = clamp(aBend, 0.0, 1.0);
    float wobble = sin(uTime * 2.6 + uPhase + aPos.x * 7.5) * 0.07;
    float ripple = sin(uTime * 6.1 + aPos.y * 9.0 + uPhase * 0.3) * 0.032;
    vec3 lp = aPos;
    lp.z += (wobble + ripple) * bend;

    vec3 n0 = normalize(aNormal);
    float nxz = 0.18 * bend * cos(uTime * 2.6 + uPhase + aPos.x * 7.5);
    vec3 n = normalize(n0 + vec3(nxz * 0.35, 0.0, 0.0));

    vec4 world = uModel * vec4(lp, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * n;
    vUv = aUv;
    vFragPosLightSpace = uLightSpaceMatrix * world;
    gl_Position = uMvp * world;
}
