#version 330 core

out vec4 FragColor;



in vec3 vWorld;

in vec3 vN;

in vec2 vUv;

in float vWet;

flat in int vMatId;

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

uniform int uTerrainMode;



#define K_POINT_LIGHTS 24

uniform int uPointLightCount;

uniform vec4 uPointLightBuf[K_POINT_LIGHTS * 2];



vec2 atlasUv(int tid, vec2 uv) {

    if (tid < 1)

        return vec2(0.0);

    const float cols = 28.0;

    float col = float(tid - 1);

    return vec2((col + fract(uv.x)) / cols, clamp(fract(uv.y), 0.001, 0.999));

}



float hash21(vec2 p) {

    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);

}



float terrainNoise(vec2 p) {

    vec2 i = floor(p);

    vec2 f = fract(p);

    f = f * f * (3.0 - 2.0 * f);

    float a = hash21(i);

    float b = hash21(i + vec2(1, 0));

    float c = hash21(i + vec2(0, 1));

    float d = hash21(i + vec2(1, 1));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);

}



vec3 triTile(int tid, vec3 wp, vec3 N) {

    vec3 w = abs(N);

    w = max(w, vec2(0.001).xxx);

    w /= (w.x + w.y + w.z);

    vec3 cx = texture(uAtlas, atlasUv(tid, wp.zy * 0.09)).rgb;

    vec3 cy = texture(uAtlas, atlasUv(tid, wp.xz * 0.09)).rgb;

    vec3 cz = texture(uAtlas, atlasUv(tid, wp.xy * 0.09)).rgb;

    return cx * w.x + cy * w.y + cz * w.z;

}



vec3 terrainAlbedo(vec3 wp, vec3 N, float wet) {

    float slope = 1.0 - clamp(N.y, 0.0, 1.0);

    float h = wp.y;

    float n1 = terrainNoise(wp.xz * 0.035);

    float n2 = terrainNoise(wp.xz * 0.11 + 17.0);



    float grassW = smoothstep(0.55, 0.12, slope) * smoothstep(22.0, 8.0, h);

    float dirtW = smoothstep(0.2, 0.45, slope) * (1.0 - grassW * 0.6);

    float rockW = smoothstep(0.38, 0.78, slope);

    float sandW = wet * 0.85 + smoothstep(6.5, 5.0, h) * 0.25;

    sandW *= smoothstep(0.35, 0.08, slope);



    float wSum = grassW + dirtW + rockW + sandW + 0.001;

    vec3 grass = triTile(1, wp + vec3(n1 * 0.4, 0, n2 * 0.4), N);

    vec3 dirt = triTile(2, wp + vec3(n2 * 0.3, 0, n1 * 0.3), N);

    vec3 rock = triTile(3, wp, N);

    vec3 sand = triTile(4, wp + vec3(n1 * 0.2, 0, n2 * 0.2), N);

    vec3 col = (grass * grassW + dirt * dirtW + rock * rockW + sand * sandW) / wSum;



    col = mix(col, sand * 0.85 + vec3(0.02, 0.06, 0.08), clamp(wet, 0.0, 1.0) * 0.55);

    return col;

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

        const float wrap = 0.28;

        float diffw = clamp((nd + wrap) / (1.0 + wrap), 0.0, 1.0);

        acc += b.xyz * att * (diffw * alb * 0.55 + 0.14 * alb) * (0.32 + 0.95 * (1.0 - dl));

    }

    return acc;

}



float shadowPoisson(vec4 fragLightSpace, vec3 N, vec3 L, vec3 worldPos, vec2 shadowTexel) {

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

        vec2 off = rot * (disk4[i] * penumbra * shadowTexel);

        sum += texture(uShadowMap, vec3(proj.xy + off, ref));

    }

    return sum * 0.25;

}



vec3 acesTonemap(vec3 x) {

    const float a = 2.51;

    const float b = 0.03;

    const float c = 2.43;

    const float d = 0.59;

    const float e = 0.14;

    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);

}



void main() {

    float dl = clamp(uDayLight, 0.06, 1.0);

    vec3 N = normalize(vN);

    bool terrain = uTerrainMode != 0;

    if (terrain) {

        float m1 = terrainNoise(vWorld.xz * 0.16) * 2.0 - 1.0;

        float m2 = terrainNoise(vWorld.xz * 0.41 + 9.3) * 2.0 - 1.0;

        N = normalize(N + vec3(m1, 0.0, m2) * 0.18);

    }



    vec3 L = normalize(-uSunDir);

    vec3 V = normalize(uViewPos - vWorld);



    vec3 alb;

    if (terrain)

        alb = terrainAlbedo(vWorld, N, clamp(vWet, 0.0, 1.0));

    else

        alb = texture(uAtlas, atlasUv(vMatId, vUv)).rgb;



    float shRaw = 1.0;

    if (!terrain)

        shRaw = shadowPoisson(vFragPosLightSpace, N, L, vWorld, uShadowTexelSize);

    float sh = mix(0.28, 1.0, shRaw);



    vec3 hemi = mix(uSkyHorizon, uSkyZenith, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));

    vec3 sunCol = uSunColor;

    if (length(sunCol) < 0.01)

        sunCol = vec3(1.1, 0.98, 0.86);

    sunCol *= mix(0.42, 1.0, dl);



    float ndl = max(dot(N, L), 0.0);

    float ndv = clamp(dot(N, V), 0.001, 1.0);



    float cavity = pow(1.0 - ndl, 1.35) * (terrain ? 0.18 : 0.32);

    float ao = clamp(1.0 - cavity, 0.55, 1.0);



    float diffK = terrain ? mix(0.62, 0.92, ndl) : mix(0.45, 1.0, sh);

    vec3 diff = sunCol * alb * ndl * diffK * ao * mix(0.35, 1.0, dl);

    vec3 amb = hemi * alb * (terrain ? uHemiStrength * 1.12 : uHemiStrength) * ao * mix(0.38, 1.0, dl);

    vec3 warmFill = vec3(0.48, 0.40, 0.34) * alb * (1.0 - ndl) * 0.14 * ao * mix(0.5, 1.0, dl);



    vec3 H = normalize(V + L);

    float specPow = terrain ? 14.0 : 28.0;

    float specK = terrain ? 0.06 : 0.22;

    float spec = pow(max(dot(N, H), 0.0), specPow) * specK * sh * mix(0.55, 1.0, dl);

    vec3 col = amb + diff + warmFill + sunCol * spec;

    col += pointLightsContrib(vWorld, N, alb, dl);



    float rim = pow(1.0 - ndv, 2.4) * (terrain ? 0.22 : 0.34);

    col += rim * mix(uSkyHorizon, sunCol, 0.52) * mix(0.45, 1.0, dl);



    float dist = length(uViewPos - vWorld);

    float fogAmt = 1.0 - exp(-dist * (terrain ? 0.00145 : 0.00175));

    vec3 fogCol = mix(uSkyHorizon * 1.05, mix(uSkyZenith, vec3(0.75, 0.88, 1.0), 0.35), 0.42);

    col = mix(col, fogCol, clamp(fogAmt, 0.0, 1.0));



    col *= mix(0.72, 1.05, dl);

    col = acesTonemap(col * 1.08);

    FragColor = vec4(pow(col, vec3(1.0 / 2.2)), 1.0);

}

