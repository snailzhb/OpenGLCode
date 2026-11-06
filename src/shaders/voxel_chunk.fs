#version 330 core
out vec4 FragColor;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUv;
in vec4 vFragPosLightSpace;
in vec4 vClipPos;
flat in int vTileId;

uniform sampler2D uSceneRefr;
uniform int uHasSceneRefr;

uniform vec3 uSunDir;
uniform vec3 uViewPos;
uniform float uEyeInWater;
uniform float uTime;
uniform int uGlassOnly;

uniform vec3 uSkyZenith;
uniform vec3 uSkyHorizon;
uniform float uDayLight;

uniform sampler2D uAtlas;
uniform sampler2D uAtlasN;
uniform int uHasAtlasN;
uniform sampler2DShadow uShadowMap;
uniform vec2 uShadowTexelSize;

#define K_POINT_LIGHTS 24
uniform int uPointLightCount;
uniform vec4 uPointLightBuf[K_POINT_LIGHTS * 2];
uniform int uLampPairCount;
uniform vec4 uLampBuf[K_POINT_LIGHTS * 2];
uniform vec3 uSunColor;
uniform float uHemiStrength;

vec3 pointLightsContrib(vec3 wp, vec3 N, vec3 alb, float dl) {
    vec3 acc = vec3(0.0);
    int n = uPointLightCount;
    if (n <= 0)
        n = uLampPairCount;
    for (int i = 0; i < K_POINT_LIGHTS; ++i) {
        if (i >= n)
            break;
        vec4 a = uPointLightCount > 0 ? uPointLightBuf[i * 2] : uLampBuf[i * 2];
        vec4 b = uPointLightCount > 0 ? uPointLightBuf[i * 2 + 1] : uLampBuf[i * 2 + 1];
        vec3 lp = a.xyz;
        float R = a.w;
        vec3 lc = b.xyz;
        vec3 to = lp - wp;
        float dist = length(to);
        if (dist > R)
            continue;
        vec3 Lm = to / max(dist, 0.001);
        float t = 1.0 - dist / R;
        float att = t * t;
        float nd = max(dot(N, Lm), 0.0);
        const float wrap = 0.32;
        float diffw = clamp((nd + wrap) / (1.0 + wrap), 0.0, 1.0);
        float night = 1.0 - dl;
        acc += lc * att * (diffw * alb * 0.62 + 0.2 * alb) * (0.28 + 1.05 * night);
    }
    return acc;
}

void matFromId(int id, out vec3 albedo, out float rough, out float metal, out float specPow) {
    rough = 0.92;
    metal = 0.0;
    specPow = 14.0;
    if (id == 1) {
        albedo = vec3(0.28, 0.66, 0.24);
        rough = 0.86;
    } else if (id == 2) {
        albedo = vec3(0.50, 0.36, 0.22);
        rough = 0.94;
    } else if (id == 3) {
        albedo = vec3(0.55, 0.55, 0.58);
        rough = 0.52;
        metal = 0.04;
        specPow = 36.0;
    } else if (id == 4) {
        albedo = vec3(0.88, 0.82, 0.58);
        rough = 0.97;
        specPow = 10.0;
    } else if (id == 6) {
        albedo = vec3(0.44, 0.30, 0.14);
        rough = 0.88;
    } else if (id == 7) {
        albedo = vec3(0.50, 0.50, 0.54);
        rough = 0.72;
        specPow = 22.0;
    } else if (id == 8) {
        albedo = vec3(0.84, 0.93, 0.97);
        rough = 0.05;
        metal = 0.0;
        specPow = 112.0;
    } else if (id == 9) {
        albedo = vec3(0.92, 0.96, 1.0);
        rough = 0.06;
        specPow = 108.0;
    } else if (id == 10) {
        albedo = vec3(0.18, 0.22, 0.34);
        rough = 0.08;
        specPow = 96.0;
    } else if (id == 11) {
        albedo = vec3(0.88, 0.9, 0.95);
        rough = 0.55;
        specPow = 28.0;
    } else if (id == 12) {
        albedo = vec3(0.92, 0.55, 0.28);
        rough = 0.55;
        specPow = 28.0;
    } else if (id == 13) {
        albedo = vec3(0.28, 0.82, 0.88);
        rough = 0.55;
        specPow = 28.0;
    } else if (id == 14) {
        albedo = vec3(0.88, 0.32, 0.92);
        rough = 0.55;
        specPow = 28.0;
    } else if (id == 15) {
        albedo = vec3(0.62, 0.22, 0.15);
        rough = 0.88;
        specPow = 20.0;
    } else if (id == 16) {
        albedo = vec3(0.68, 0.55, 0.18);
        rough = 0.93;
        specPow = 11.0;
    } else if (id == 17) {
        albedo = vec3(0.45, 0.43, 0.42);
        rough = 0.96;
        specPow = 12.0;
    } else if (id == 18) {
        albedo = vec3(0.28, 0.34, 0.26);
        rough = 0.82;
        specPow = 18.0;
    } else if (id == 19) {
        albedo = vec3(0.58, 0.25, 0.15);
        rough = 0.62;
        specPow = 24.0;
    } else if (id == 20) {
        albedo = vec3(0.28, 0.32, 0.38);
        rough = 0.58;
        specPow = 28.0;
    } else if (id == 21) {
        albedo = vec3(0.52, 0.36, 0.20);
        rough = 0.82;
        specPow = 18.0;
    } else if (id == 22) {
        albedo = vec3(0.88, 0.86, 0.84);
        rough = 0.22;
        metal = 0.05;
        specPow = 48.0;
    } else if (id == 23) {
        albedo = vec3(0.48, 0.50, 0.54);
        rough = 0.35;
        metal = 0.72;
        specPow = 64.0;
    } else if (id == 24) {
        albedo = vec3(0.32, 0.38, 0.30);
        rough = 0.78;
        specPow = 20.0;
    } else if (id == 25) {
        albedo = vec3(0.32, 0.62, 0.22);
        rough = 0.90;
        specPow = 10.0;
    } else if (id == 26) {
        albedo = vec3(0.55, 0.28, 0.16);
        rough = 0.68;
        specPow = 22.0;
    } else if (id == 27) {
        albedo = vec3(0.22, 0.14, 0.08);
        rough = 0.86;
        specPow = 16.0;
    } else {
        albedo = vec3(0.55, 0.25, 0.75);
    }
}

float detailStrength(int id) {
    if (id == 8 || id == 9 || id == 10)
        return 0.06;
    if (id >= 15 && id <= 19)
        return 0.58;
    if (id >= 11 && id <= 14)
        return 0.22;
    if (id == 3 || id == 7)
        return 0.52;
    if (id == 1 || id == 2 || id == 6)
        return 0.62;
    if (id == 4)
        return 0.56;
    return 0.46;
}

vec2 atlasUv(int tid, vec2 uv) {
    if (tid < 1)
        return vec2(0.0);
    const float cols = 28.0;
    float col = float(tid - 1);
    return vec2((col + clamp(uv.x, 0.001, 0.999)) / cols, clamp(uv.y, 0.001, 0.999));
}

/** 按相机距离选手动 mip：近处锁 0 层，避免大三角上 ddx 过大导致糊成一团 */
float atlasLodFromDist(float camDist) {
    float x = camDist * 0.095 + 0.045;
    if (x <= 1.0)
        return 0.0;
    return min(floor(log2(x) + 0.38), 4.0);
}

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm2(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    mat2 m = mat2(1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < 3; ++i) {
        v += a * vnoise2(p);
        p = m * p;
        a *= 0.5;
    }
    return v;
}

vec2 faceUv(vec3 N, vec3 wp) {
    vec3 an = abs(N);
    if (an.y > an.x && an.y > an.z)
        return wp.xz;
    if (an.x > an.z)
        return wp.zy;
    return wp.xy;
}

vec3 perturbNormal(vec3 N, vec3 wp, float strength, bool hiDetail) {
    vec2 uv = faceUv(N, wp) * 2.7;
    const float e = 0.11;
    float c = fbm2(uv);
    float dx = fbm2(uv + vec2(e, 0.0)) - c;
    float dy = fbm2(uv + vec2(0.0, e)) - c;
    vec3 up = vec3(0.0, 1.0, 0.0);
    vec3 T = cross(N, up);
    if (dot(T, T) < 1e-6)
        T = cross(N, vec3(1.0, 0.0, 0.0));
    T = normalize(T);
    vec3 B = normalize(cross(N, T));
    vec3 gn = T * dx + B * dy;
    if (hiDetail) {
        vec2 uvf = uv * 6.15;
        const float e2 = 0.036;
        float c2 = fbm2(uvf);
        float dx2 = fbm2(uvf + vec2(e2, 0.0)) - c2;
        float dy2 = fbm2(uvf + vec2(0.0, e2)) - c2;
        gn += T * (dx2 * 0.65) + B * (dy2 * 0.65);
    }
    return normalize(N - gn * strength * (hiDetail ? 1.1 : 1.0));
}

float shadowPoisson(vec4 fragLightSpace, vec3 N, vec3 L, vec3 worldPos, vec2 shadowTexel) {
    vec3 proj = fragLightSpace.xyz / fragLightSpace.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0)
        return 1.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0;

    float ndl = max(dot(N, L), 0.0);
    float bias = max(0.00012, 0.00095 * (1.0 - ndl));
    float ref = proj.z - bias;

    float rnd = fract(sin(dot(worldPos.xz + worldPos.y * 0.07, vec2(127.1, 311.7))) * 43758.5453);
    float ang = rnd * 6.28318530718;
    float co = cos(ang);
    float si = sin(ang);
    mat2 rot = mat2(co, -si, si, co);

    const vec2 disk4[4] = vec2[](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
    float sum = 0.0;
    const float penumbra = 2.75;
    for (int i = 0; i < 4; ++i) {
        vec2 off = rot * (disk4[i] * penumbra * shadowTexel);
        sum += texture(uShadowMap, vec3(proj.xy + off, ref));
    }
    return sum * 0.25;
}

vec3 emissiveFromId(int id) {
    if (id == 11)
        return vec3(1.25, 1.32, 1.42);
    if (id == 12)
        return vec3(1.55, 0.95, 0.38);
    if (id == 13)
        return vec3(0.32, 1.45, 1.52);
    if (id == 14)
        return vec3(1.42, 0.35, 1.48);
    return vec3(0.0);
}

vec3 lampRandomTint(vec3 wp) {
    vec2 p = wp.xz * 0.37 + wp.y * 0.09;
    float t = fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    vec3 a = vec3(1.0, 0.72, 0.38);
    vec3 b = vec3(0.42, 0.95, 1.05);
    vec3 c = vec3(1.05, 0.45, 0.92);
    vec3 d = vec3(0.55, 1.0, 0.52);
    vec3 m = mix(mix(a, b, t), mix(c, d, fract(t * 7.13)), fract(t * 13.7));
    return clamp(m * (0.88 + 0.24 * fract(t * 59.1)), 0.2, 2.2);
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
    vec3 N0 = normalize(vNormal);
    vec3 V = normalize(uViewPos - vWorldPos);
    vec3 L = normalize(-uSunDir);
    vec3 sunCol = uSunColor;
    if (length(sunCol) < 0.01)
        sunCol = vec3(1.14, 1.02, 0.86);
    sunCol *= mix(0.36, 1.0, dl);
    vec3 hemiUp = mix(uSkyHorizon, uSkyZenith, clamp(N0.y * 0.5 + 0.5, 0.0, 1.0));

    float shRaw = shadowPoisson(vFragPosLightSpace, N0, L, vWorldPos, uShadowTexelSize);
    bool isEmissive = (vTileId >= 11 && vTileId <= 14);
    if (isEmissive)
        shRaw = max(shRaw, 0.88);
    float shSun = mix(0.26, 1.0, shRaw);

    float camDist = length(uViewPos - vWorldPos);
    bool hiNrm = camDist < 26.0;
    float atlasLod = atlasLodFromDist(camDist);
    vec2 uvAtlas = atlasUv(vTileId, vUv);
    vec3 texRgb = textureLod(uAtlas, uvAtlas, atlasLod).rgb;
    float texL = dot(texRgb, vec3(0.299, 0.587, 0.114));

    if (uGlassOnly != 0) {
        if (vTileId < 8 || vTileId > 10)
            discard;

        vec3 Ng = normalize(N0);
        vec3 Nface = (dot(Ng, V) < 0.0) ? -Ng : Ng;
        vec3 N = perturbNormal(Nface, vWorldPos, detailStrength(vTileId) * 0.18, camDist < 26.0);

        float cosTheta = clamp(abs(dot(N, V)), 0.015, 1.0);
        float F0 = (vTileId == 10) ? 0.062 : 0.034;
        float Fresnel = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
        float edge = pow(1.0 - cosTheta, 3.2);

        vec3 tint;
        float rough, metal, specPow;
        matFromId(vTileId, tint, rough, metal, specPow);
        float texBlend = vTileId == 10 ? 0.18 : (vTileId == 9 ? 0.48 : 0.55);
        vec3 alb = mix(tint, tint * texRgb * 1.06, texBlend);

        float w = max(abs(vClipPos.w), 1e-4);
        vec2 uv = vClipPos.xy / w * 0.5 + 0.5;
        float eta = vTileId == 10 ? 0.055 : 0.078;
        vec2 refrOff = (N.xy * (1.0 - abs(N.z) * 0.35) + vec2(N.z * N.x, N.z * N.y) * 0.25) * eta;
        refrOff += vec2(sin(vWorldPos.y * 2.1 + uTime * 1.1), cos(vWorldPos.x * 2.0 - uTime * 0.9)) * 0.008;
        vec2 uvR = clamp(uv + refrOff, 0.02, 0.98);

        vec3 absorp = vTileId == 10 ? vec3(0.82, 0.88, 1.0) : (vTileId == 9 ? vec3(0.92, 0.97, 1.0) : vec3(0.88, 0.98, 0.95));
        vec3 sceneBehind;
        if (uHasSceneRefr != 0)
            sceneBehind = textureLod(uSceneRefr, uvR, 0.0).rgb * absorp;
        else
            sceneBehind = mix(uSkyHorizon, hemiUp, uvR.y) * absorp * (0.92 + 0.08 * hash12(uvR * 300.0));
        sceneBehind *= mix(0.55, 1.0, dl);
        float caust = 0.06 * sin(uvR.x * 55.0 + uTime * 1.4) * sin(uvR.y * 48.0 - uTime * 1.1);
        sceneBehind *= 1.0 + caust;

        vec3 H = normalize(V + L);
        float ndl = max(dot(N, L), 0.0);
        vec3 sunDiff = sunCol * alb * ndl * 1.05 * shSun * mix(0.35, 1.0, dl);

        vec3 R = reflect(-V, N);
        float up = max(R.y, 0.001);
        vec3 skyRef = mix(uSkyHorizon, uSkyZenith, pow(up, 0.48));
        float sunGl = pow(max(dot(normalize(R), L), 0.0), 72.0);
        vec3 refl = mix(skyRef, sunCol * 1.12, 0.28 * sunGl);
        float spec = pow(max(dot(N, H), 0.0), specPow * 0.68) * shSun;
        vec3 specCol = sunCol * spec * (0.55 + Fresnel * 1.35) + vec3(0.35, 0.38, 0.42) * pow(max(dot(N, H), 0.0), specPow * 0.35) * 0.45;
        specCol *= mix(0.45, 1.0, dl);

        vec3 edgeCol = mix(vec3(0.65, 0.82, 1.0), vec3(1.0, 0.96, 0.86), 0.45);
        float T = clamp(1.0 - Fresnel * 0.78 - edge * 0.08, 0.0, 1.0);
        vec3 col = sceneBehind * T + refl * Fresnel * 0.88;
        col += sunDiff * (0.22 + (1.0 - T) * 0.35);
        col += edgeCol * edge * 0.55;
        col += specCol;
        col += vec3(0.02, 0.03, 0.045) * (1.0 - Fresnel) * (vTileId == 10 ? 0.42 : 0.18);

        col += pointLightsContrib(vWorldPos, N, alb, dl) * 0.48;

        float alphaMin = vTileId == 10 ? 0.22 : (vTileId == 9 ? 0.16 : 0.14);
        float alpha = clamp(alphaMin + Fresnel * 0.62 + edge * 0.32 + length(specCol) * 0.06, 0.12, 0.88);

        col *= mix(0.38, 1.0, dl);
        FragColor = vec4(acesTonemap(col * 1.1), alpha);
        return;
    }

    if (vTileId >= 8 && vTileId <= 10)
        discard;

    vec3 Ngeo = N0;
    if (uHasAtlasN != 0) {
        vec3 nm = textureLod(uAtlasN, uvAtlas, atlasLod).xyz * 2.0 - 1.0;
        vec3 up = vec3(0.0, 1.0, 0.0);
        vec3 Tm = cross(N0, up);
        if (dot(Tm, Tm) < 1e-8)
            Tm = cross(N0, vec3(1.0, 0.0, 0.0));
        Tm = normalize(Tm);
        vec3 Bm = normalize(cross(N0, Tm));
        vec3 Nmap = normalize(Tm * nm.x + Bm * nm.y + N0 * clamp(nm.z, 0.25, 1.0));
        Ngeo = normalize(mix(N0, Nmap, 0.5));
    }

    vec3 albedo;
    float rough, metal, specPow;
    matFromId(vTileId, albedo, rough, metal, specPow);

    albedo = mix(albedo, albedo * texRgb * 1.38, 0.9);
    rough = clamp(rough + (1.0 - texL) * 0.22 - texL * 0.06, 0.02, 0.99);
    specPow = mix(specPow, specPow * 0.82, (1.0 - texL) * 0.5);

    if (vTileId == 1) {
        float tops = clamp(N0.y, 0.0, 1.0);
        vec3 sideTint = vec3(0.42, 0.30, 0.18);
        albedo = mix(sideTint * 0.95, albedo, 0.28 + 0.72 * tops);
    }

    float micro = fract(sin(dot(floor(vWorldPos.xz * 4.0), vec2(12.9898, 78.233))) * 43758.5453);
    albedo *= 0.97 + 0.05 * micro;

    vec3 N = perturbNormal(Ngeo, vWorldPos, detailStrength(vTileId), hiNrm);
    vec3 H = normalize(V + L);

    float aoTex = mix(0.58, 1.0, smoothstep(0.12, 0.62, texL));
    float cavity = pow(1.0 - max(dot(N, L), 0.0), 1.35) * 0.28 * (1.0 - abs(N0.y) * 0.35);
    float ao = clamp(aoTex * (1.0 - cavity), 0.35, 1.0);

    float ndl = max(dot(N, L), 0.0);
    float ndv = clamp(dot(N, V), 0.001, 1.0);
    float ndh = max(dot(N, H), 0.0);
    float spec = pow(ndh, specPow) * (0.12 + (1.0 - rough) * 0.72 + metal * 0.5);

    float skyOcc = clamp(N0.y * 0.35 + 0.65, 0.0, 1.0);
    vec3 ambSky = hemiUp * albedo * (0.42 + 0.28 * skyOcc) * ao * mix(0.32, 1.0, dl);
    vec3 amb = vec3(0.10, 0.11, 0.13) * albedo * (0.45 + 0.35 * skyOcc) * ao * mix(0.45, 1.0, dl);
    vec3 diff = sunCol * albedo * ndl * 2.15 * shSun * ao * mix(0.28, 1.0, dl);
    vec3 warmFill = vec3(0.52, 0.44, 0.36) * albedo * (1.0 - ndl) * 0.16 * ao;
    vec3 skyFill = vec3(0.32, 0.38, 0.48) * albedo * (1.0 - ndl) * 0.10 * ao;
    if (isEmissive) {
        warmFill *= 1.65;
        skyFill *= 1.65;
    }
    vec3 col = amb + ambSky + diff + warmFill + skyFill + sunCol * spec * 1.35 * shSun * mix(1.0, ao, 0.5);
    vec3 lampGlow = pointLightsContrib(vWorldPos, N, albedo, dl);
    if (isEmissive)
        lampGlow *= 0.2;
    col += lampGlow;

    float rim = pow(1.0 - ndv, 2.6) * (1.0 - rough) * 0.32;
    col += rim * mix(uSkyHorizon, sunCol, 0.55);

    float dist = length(uViewPos - vWorldPos);
    float fogAmt = (1.0 - exp(-dist * 0.00175)) * (1.0 - uEyeInWater * 0.4);
    vec3 fogCol = mix(uSkyHorizon * 1.05, mix(uSkyZenith, vec3(0.75, 0.88, 1.0), 0.35), 0.42);
    col = mix(col, fogCol, clamp(fogAmt, 0.0, 1.0));

    float uw = clamp(uEyeInWater, 0.0, 1.0);
    vec3 deep = vec3(0.07, 0.24, 0.34);
    float caust = 0.045 * sin(vWorldPos.x * 2.0 + uTime * 1.5) * sin(vWorldPos.z * 2.2 + uTime * 1.2);
    col = mix(col, deep * (0.93 + caust), uw * 0.42);
    col += vec3(0.025, 0.1, 0.12) * uw;

    vec3 em = emissiveFromId(vTileId);
    if (isEmissive) {
        em *= lampRandomTint(vWorldPos);
        em *= (1.0 + (1.0 - dl) * 1.15);
    }
    vec3 baseCol = col;
    col = baseCol + em;
    col *= 1.22;
    vec3 mapped = acesTonemap(col);
    if (isEmissive) {
        vec3 emOnly = acesTonemap(em * 0.42);
        mapped = mapped * 0.82 + emOnly * (0.55 + 0.45 * (1.0 - dl));
    }
    FragColor = vec4(pow(mapped, vec3(1.0 / 1.82)), 1.0);
}
