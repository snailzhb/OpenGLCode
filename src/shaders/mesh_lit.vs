#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aUv;
layout(location = 6) in float aWet;

layout(location = 3) in vec4 iCenter;
layout(location = 4) in vec4 iHalf;
layout(location = 5) in vec4 iMatYaw;

uniform mat4 uMvp;
uniform mat4 uModel;
uniform mat4 uLightSpace;
uniform int uInstanced;

out vec3 vWorld;
out vec3 vN;
out vec2 vUv;
out float vWet;
flat out int vMatId;
out vec4 vFragPosLightSpace;

void main() {
    vec3 pos = aPos;
    vec3 nrm = aNrm;
    if (uInstanced != 0) {
        float yaw = iMatYaw.y;
        float c = cos(radians(yaw));
        float s = sin(radians(yaw));
        pos = vec3(c * aPos.x - s * aPos.z, aPos.y, s * aPos.x + c * aPos.z);
        nrm = vec3(c * aNrm.x - s * aNrm.z, aNrm.y, s * aNrm.x + c * aNrm.z);
        pos = pos * (iHalf.xyz * 2.0) + iCenter.xyz;
        vWet = 0.0;
    } else {
        vec4 wp = uModel * vec4(aPos, 1.0);
        pos = wp.xyz;
        nrm = mat3(uModel) * aNrm;
        vWet = aWet;
    }
    vWorld = pos;
    vN = nrm;
    vUv = aUv;
    vMatId = uInstanced != 0 ? int(iMatYaw.x + 0.5) : 0;
    vFragPosLightSpace = uLightSpace * vec4(pos, 1.0);
    gl_Position = uMvp * vec4(pos, 1.0);
}
