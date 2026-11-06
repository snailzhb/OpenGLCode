#version 330 core

layout(location = 0) in vec3 aPos;

layout(location = 6) in float aWet;



uniform mat4 uMvp;

uniform float uTime;



out vec3 vWorld;

out vec3 vNormal;

out float vWet;

out float vWave;



void main() {

    vec3 base = aPos;

    float w = aWet;



    float t1 = uTime * 1.15;

    float t2 = uTime * 0.82;

    float ripple1 = sin(base.x * 0.22 + t1) * cos(base.z * 0.19 + t1 * 0.88) * 0.11 * w;

    float ripple2 = sin(base.x * 0.41 - t2) * cos(base.z * 0.37 + t2 * 1.1) * 0.05 * w;

    float ripple = ripple1 + ripple2;



    float dr_dx = 0.22 * cos(base.x * 0.22 + t1) * cos(base.z * 0.19 + t1 * 0.88) * 0.11 * w

              - 0.41 * cos(base.x * 0.41 - t2) * cos(base.z * 0.37 + t2 * 1.1) * 0.05 * w;

    float dr_dz = -0.19 * sin(base.x * 0.22 + t1) * sin(base.z * 0.19 + t1 * 0.88) * 0.11 * w

               - 0.37 * sin(base.x * 0.41 - t2) * sin(base.z * 0.37 + t2 * 1.1) * 0.05 * w;



    vec3 p = base;

    p.y += ripple;

    vWorld = p;

    vNormal = normalize(vec3(-dr_dx, 1.0, -dr_dz));

    vWet = w;

    vWave = ripple;

    gl_Position = uMvp * vec4(p, 1.0);

}

