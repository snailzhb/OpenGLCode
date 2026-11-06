#version 330 core
out vec4 FragColor;

in vec3 vWorld;
in vec3 vN;
in vec2 vUv;
flat in int vTileId;
in vec4 vFragPosLightSpace;

uniform sampler2D uAtlas;
uniform sampler2DShadow uShadowMap;
uniform vec2 uShadowTexelSize;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uViewPos;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform float uDayLight;
uniform float uHemiStrength;

#define K_POINT_LIGHTS 24
uniform int uPointLightCount;
uniform vec4 uPointLightBuf[K_POINT_LIGHTS * 2];

float shadowPoisson(vec4 fragLightSpace, vec3 N, vec3 L, vec3 worldPos, vec2 shadowTexel) {
    vec3 proj = fragLightSpace.xyz / fragLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0)
        return 1.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;
    float ndl = max(dot(N, L), 0.0);
    float bias = max(0.00015, 0.001 * (1.0 - ndl));
    float ref = proj.z - bias;

    float rnd = fract(sin(dot(worldPos.xz + worldPos.y * 0.07, vec2(127.1, 311.7))) * 43758.5453);
    float ang = rnd * 6.28318530718;
    mat2 rot = mat2(cos(ang), -sin(ang), sin(ang), cos(ang));
    const vec2 disk4[4] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    float sum = 0.0;
    for (int i = 0; i < 4; ++i)
        sum += texture(uShadowMap, vec3(proj.xy + rot * (disk4[i] * 2.4 * shadowTexel), ref));
    return sum * 0.25;
}

vec2 atlasUv(int tid, vec2 uv) {
    if (tid < 1)
        return vec2(0.0);
    const float cols = 28.0;
    float col = float(tid - 1);
    return vec2((col + clamp(uv.x, 0.001, 0.999)) / cols, clamp(uv.y, 0.001, 0.999));
}

vec3 pointLightsContrib(vec3 wp, vec3 N, vec3 alb, float dl) {
    vec3 acc = vec3(0.0);
    for (int i = 0; i < K_POINT_LIGHTS; ++i) {
        if (i >= uPointLightCount)
            break;
        vec4 a = uPointLightBuf[i * 2];
        vec4 b = uPointLightBuf[i * 2 + 1];
        vec3 to = a.xyz - wp;
        float dist = length(to);
        if (dist > a.w)
            continue;
        vec3 Lm = to / max(dist, 0.001);
        float t = 1.0 - dist / a.w;
        float att = t * t;
        float nd = max(dot(N, Lm), 0.0);
        float night = 1.0 - dl;
        acc += b.xyz * att * (nd * alb * 0.55 + 0.18 * alb) * (0.35 + 0.9 * night);
    }
    return acc;
}

vec3 acesTonemap(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 N = normalize(vN);
    vec3 L = normalize(-uSunDir);
    vec3 V = normalize(uViewPos - vWorld);
    vec2 uva = atlasUv(vTileId, vUv);
    vec3 alb = texture(uAtlas, uva).rgb;

    float dl = clamp(uDayLight, 0.06, 1.0);
    float shRaw = shadowPoisson(vFragPosLightSpace, N, L, vWorld, uShadowTexelSize);
    float sh = mix(0.26, 1.0, shRaw);
    float ndl = max(dot(N, L), 0.0);
    float ndv = clamp(dot(N, V), 0.001, 1.0);
    vec3 hemi = mix(uSkyHorizon, uSkyZenith, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 sunCol = uSunColor;
    if (length(sunCol) < 0.01)
        sunCol = vec3(1.1, 0.98, 0.86);
    sunCol *= mix(0.42, 1.0, dl);

    vec3 diff = sunCol * alb * ndl * mix(0.35, 1.0, sh) * mix(0.32, 1.0, dl);
    vec3 amb = hemi * alb * uHemiStrength * mix(0.38, 1.0, dl);
    vec3 H = normalize(V + L);
    float spec = pow(max(dot(N, H), 0.0), 24.0) * 0.32 * sh * mix(0.55, 1.0, dl);
    vec3 col = amb + diff + sunCol * spec;
    col += pointLightsContrib(vWorld, N, alb, dl);

    float rim = pow(1.0 - ndv, 2.5) * 0.28;
    col += rim * mix(uSkyHorizon, sunCol, 0.5) * mix(0.45, 1.0, dl);

    float dist = length(uViewPos - vWorld);
    float fogAmt = 1.0 - exp(-dist * 0.00175);
    vec3 fogCol = mix(uSkyHorizon * 1.05, mix(uSkyZenith, vec3(0.75, 0.88, 1.0), 0.35), 0.42);
    col = mix(col, fogCol, clamp(fogAmt, 0.0, 1.0));

    col *= mix(0.72, 1.0, dl);
    col = acesTonemap(col * 1.05);
    FragColor = vec4(pow(col, vec3(1.0 / 2.2)), 1.0);
}
