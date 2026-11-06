#version 330 core
in vec3 vN;
in vec3 vP;
out vec4 FragColor;

uniform vec3 uSunDir;
uniform vec3 uEmissive;
uniform float uDayLight;

void main() {
    vec3 N = normalize(vN);
    vec3 L = normalize(-uSunDir);
    float ndl = max(dot(N, L), 0.0);
    float dl = clamp(uDayLight, 0.0, 1.0);
    vec3 base = vec3(0.22, 0.23, 0.26) * (0.18 + 0.82 * ndl) * mix(0.35, 1.0, dl);
    vec3 glow = uEmissive * (0.55 + 0.45 * ndl) * mix(0.4, 1.15, dl);
    vec3 c = base + glow * 0.85;
    FragColor = vec4(c, 1.0);
}
