#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 3) in vec4 iCenter;
layout(location = 4) in vec4 iHalf;
layout(location = 5) in vec4 iMatYaw;
uniform mat4 uLightMvp;
uniform mat4 uModel;
uniform int uInstanced;

void main() {
    vec3 pos = aPos;
    if (uInstanced != 0) {
        float yaw = iMatYaw.y;
        float c = cos(radians(yaw));
        float s = sin(radians(yaw));
        pos = vec3(c * aPos.x - s * aPos.z, aPos.y, s * aPos.x + c * aPos.z);
        pos = pos * (iHalf.xyz * 2.0) + iCenter.xyz;
    } else {
        pos = (uModel * vec4(aPos, 1.0)).xyz;
    }
    gl_Position = uLightMvp * vec4(pos, 1.0);
}
