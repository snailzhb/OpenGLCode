#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv;
in vec4 vFragPosLightSpace;

uniform vec3 uSunDir;
uniform vec3 uViewPos;
uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform int uColorSeed;
uniform sampler2DShadow uShadowMap;
uniform vec2 uShadowTexelSize;
uniform float uDayLight;

float shadowPoisson(vec4 fragLightSpace, vec3 N, vec3 L, vec3 worldPos) {
    vec3 proj = fragLightSpace.xyz / fragLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0)
        return 1.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    float ndl = max(dot(N, L), 0.0);
    float bias = max(0.00015, 0.0011 * (1.0 - ndl));
    float ref = proj.z - bias;

    float rnd = fract(sin(dot(worldPos.xz + worldPos.y * 0.07, vec2(127.1, 311.7))) * 43758.5453);
    float ang = rnd * 6.28318530718;
    float co = cos(ang);
    float si = sin(ang);
    mat2 rot = mat2(co, -si, si, co);

    const vec2 disk4[4] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    float sum = 0.0;
    const float penumbra = 2.5;
    for (int i = 0; i < 4; ++i) {
        vec2 off = rot * (disk4[i] * penumbra * uShadowTexelSize);
        sum += texture(uShadowMap, vec3(proj.xy + off, ref));
    }
    return sum * 0.25;
}

vec3 flagPalette(int seed, vec2 uv) {
    vec3 a = vec3(0.78, 0.16, 0.18);
    vec3 b = vec3(0.12, 0.22, 0.72);
    vec3 c = vec3(0.14, 0.62, 0.28);
    vec3 base = (seed == 1) ? b : ((seed == 2) ? c : a);
    float stripe = step(0.5, fract(uv.y * 5.3 + uv.x * 0.7));
    vec3 alt = mix(base, base * vec3(1.12, 1.08, 0.92), 0.35);
    return mix(base, alt, stripe);
}

void main() {
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(-uSunDir);
    vec3 Ng = normalize(vNormal);

    /* 视觉法线：总朝向相机，薄片两面观感一致（避免 gl_FrontFacing + 插值法线导致一面过暗） */
    vec3 Nvis = (dot(Ng, V) < 0.0) ? -Ng : Ng;
    /* 阴影 bias 用法线朝向太阳，与深度图一致、减轻自阴影误差 */
    vec3 Nbias = (dot(Ng, L) < 0.0) ? -Ng : Ng;

    vec3 base = flagPalette(uColorSeed, vUv);

    float sh = shadowPoisson(vFragPosLightSpace, Nbias, L, vWorldPos);
    float ndl = max(dot(Nvis, L), 0.0);
    /* 略 wrap，布面暗侧不会死黑 */
    const float wrap = 0.28;
    float sun = clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0) * mix(0.22, 1.0, sh);
    vec3 hemi = mix(uSkyHorizon, uSkyZenith, clamp(Nvis.y * 0.5 + 0.35, 0.0, 1.0));
    float diff = sun * 0.82 + 0.26 * hemi.r;
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(Nvis, H), 0.0), 40.0) * 0.06 * sh;

    vec3 col = base * diff + vec3(1.0, 0.95, 0.82) * spec;
    float dl = clamp(uDayLight, 0.0, 1.0);
    col *= mix(0.34, 1.0, dl);
    FragColor = vec4(col, 1.0);
}
