#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 uMvp;

out vec3 vN;
out vec3 vP;

void main() {
    vP = aPos;
    vN = aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
