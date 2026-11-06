#version 330 core

out vec4 FragColor;



in vec3 vWorld;

in vec3 vNormal;

in float vWet;

in float vWave;



uniform vec3 uViewPos;

uniform vec3 uSunDir;

uniform vec3 uSunColor;

uniform vec3 uSkyHorizon;

uniform vec3 uSkyZenith;

uniform float uDayLight;

uniform float uTime;



float hash12(vec2 p) {

    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);

}



void main() {

    vec3 N = normalize(vNormal);

    vec3 V = normalize(uViewPos - vWorld);

    vec3 L = normalize(-uSunDir);

    float dl = clamp(uDayLight, 0.06, 1.0);

    float depth = clamp(vWet, 0.0, 1.0);

    float shallow = 1.0 - depth;



    vec3 shallowCol = vec3(0.16, 0.58, 0.68);

    vec3 deepCol = vec3(0.03, 0.18, 0.34);

    vec3 water = mix(deepCol, shallowCol, shallow * 0.85 + 0.15);



    float caust = 0.07 * sin(vWorld.x * 2.4 + uTime * 1.35) * sin(vWorld.z * 2.1 - uTime * 1.05);

    water *= 1.0 + caust * shallow;



    float fres = pow(1.0 - max(dot(N, V), 0.0), 4.2);

    vec3 R = reflect(-V, N);

    float up = max(R.y, 0.001);

    vec3 skyRef = mix(uSkyHorizon, uSkyZenith, pow(up, 0.55));

    float sunGl = pow(max(dot(normalize(R), L), 0.0), 64.0);

    vec3 refl = mix(skyRef, uSunColor * 1.15, 0.32 * sunGl);



    vec3 sunCol = uSunColor;

    if (length(sunCol) < 0.01)

        sunCol = vec3(1.1, 0.98, 0.86);

    sunCol *= mix(0.45, 1.0, dl);



    float spec = pow(max(dot(reflect(-V, N), L), 0.0), 112.0);

    vec3 col = mix(water, refl, fres * 0.72);

    col += sunCol * spec * 0.85;



    float foamNoise = hash12(floor(vWorld.xz * 3.2) + floor(uTime * 2.5));

    float foamEdge = smoothstep(0.72, 0.98, shallow) * smoothstep(0.02, 0.18, abs(vWave) + foamNoise * 0.08);

    col = mix(col, vec3(0.92, 0.96, 0.98), foamEdge * 0.55);



    float dist = length(uViewPos - vWorld);

    float fogAmt = 1.0 - exp(-dist * 0.0016);

    vec3 fogCol = mix(uSkyHorizon * 0.95, uSkyZenith * 0.88, 0.45);

    col = mix(col, fogCol, clamp(fogAmt, 0.0, 1.0));



    col *= mix(0.68, 1.05, dl);

    float alpha = mix(0.58, 0.86, depth) + fres * 0.12;

    alpha = clamp(alpha, 0.45, 0.92);

    FragColor = vec4(col, alpha);

}

