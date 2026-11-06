#version 330 core
in vec3 vWorld;
out vec4 FragColor;

uniform vec3 uCamPos;
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform vec3 uSunDirWorld;
uniform float uSunBloom;
uniform float uDayLight;

void main() {
    float dl = clamp(uDayLight, 0.0, 1.0);
    vec3 d = normalize(vWorld - uCamPos);
    float h = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 col = mix(uHorizon, uZenith, pow(1.0 - h, 1.05));
    col = mix(col, col * vec3(0.96, 1.02, 1.12), 0.18);

    vec3 Ls = normalize(uSunDirWorld);
    float sd = clamp(dot(d, Ls), -1.0, 1.0);

    /* 朝太阳方向的天空微暖色 */
    float towardSun = pow(max(sd, 0.0), 3.0);
    col += vec3(0.14, 0.08, 0.02) * towardSun * 0.35 * mix(0.25, 1.0, dl);

    float sunVis = smoothstep(0.12, 0.52, dl);
    /* 清晰日盘：smoothstep 在 cos 空间上给约 2.5°～4° 视半径 */
    float diskInner = smoothstep(0.9971, 0.9993, sd);
    float diskEdge = smoothstep(0.9945, 0.9978, sd);
    vec3 sunCore = vec3(1.0, 0.98, 0.92) * diskInner * (1.15 + uSunBloom * 0.35) * sunVis;
    vec3 sunLim = vec3(1.0, 0.88, 0.55) * (diskEdge - diskInner * 0.85) * 0.95 * sunVis;
    col += sunCore + sunLim;

    /* 内光晕 */
    col += vec3(1.0, 0.92, 0.72) * pow(max(sd, 0.0), 96.0) * uSunBloom * 0.95 * sunVis;
    /* 外光晕（大角度，像日冕） */
    col += vec3(1.0, 0.82, 0.58) * pow(max(sd, 0.0), 18.0) * uSunBloom * 0.22 * sunVis;
    col += vec3(0.95, 0.75, 0.45) * pow(max(sd, 0.0), 6.0) * uSunBloom * 0.08 * sunVis;

    /* 夜空稀疏星点 */
    float night = 1.0 - dl;
    if (night > 0.08) {
        vec2 sp = d.xz * 140.0 + d.y * 90.0;
        float st = step(0.994, fract(sin(dot(floor(sp), vec2(12.9898, 78.233))) * 43758.5453));
        col += vec3(0.45, 0.52, 0.95) * st * night * 0.9;
    }

    col *= mix(0.32, 1.12, pow(dl, 0.55));
    col = min(col, vec3(3.8));
    FragColor = vec4(col, 1.0);
}
