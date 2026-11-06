#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 iPosScale;
layout(location = 4) in vec4 iTile;

uniform mat4 uMvp;
uniform mat4 uLightSpace;

out vec3 vWorld;
out vec3 vN;
out vec2 vUv;
flat out int vTileId;
out vec4 vFragPosLightSpace;

void main() {
    vec3 lp = aPos;
    vec3 ln = aNrm;
    float yaw = iTile.y;
    if (abs(yaw) > 0.01) {
        float rad = radians(yaw);
        float c = cos(rad);
        float s = sin(rad);
        lp = vec3(c * aPos.x - s * aPos.z, aPos.y, s * aPos.x + c * aPos.z);
        ln = vec3(c * aNrm.x - s * aNrm.z, aNrm.y, s * aNrm.x + c * aNrm.z);
    }
    vec3 wp = iPosScale.xyz + lp * iPosScale.w;
    vWorld = wp;
    vN = ln;
    vUv = aUv;
    vTileId = int(iTile.x + 0.5);
    vFragPosLightSpace = uLightSpace * vec4(wp, 1.0);
    gl_Position = uMvp * vec4(wp, 1.0);
}
