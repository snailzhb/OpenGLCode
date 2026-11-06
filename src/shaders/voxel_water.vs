#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aTileId;

uniform mat4 uMvp;
uniform float uTime;

out vec3 vWorld;
out vec3 vNormal;
out vec4 vClip;

void main() {
    float wx = aPos.x;
    float wz = aPos.z;
    float w = sin(wx * 2.6 + uTime * 1.1) * 0.045 + cos(wz * 3.0 + uTime * 0.95) * 0.038
            + sin((wx + wz) * 1.9 + uTime * 0.75) * 0.022;
    vec3 p = vec3(aPos.x, aPos.y + w, aPos.z);
    vec3 n0 = vec3(-cos(wx * 2.6 + uTime * 1.1) * 0.12, 1.0, sin(wz * 3.0 + uTime * 0.95) * 0.11);
    vNormal = normalize(n0);
    vWorld = p;
    vClip = uMvp * vec4(p, 1.0);
    gl_Position = vClip;
}
