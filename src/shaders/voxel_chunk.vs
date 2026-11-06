#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in float aTileId;

uniform mat4 uMvp;
uniform mat4 uLightSpaceMatrix;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUv;
out vec4 vFragPosLightSpace;
out vec4 vClipPos;
flat out int vTileId;

void main() {
    vWorldPos = aPos;
    vNormal = aNormal;
    vUv = aUv;
    vTileId = int(aTileId + 0.5);
    vFragPosLightSpace = uLightSpaceMatrix * vec4(aPos, 1.0);
    vClipPos = uMvp * vec4(aPos, 1.0);
    gl_Position = vClipPos;
}
