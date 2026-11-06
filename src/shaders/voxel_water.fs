#version 330 core
out vec4 FragColor;

in vec3 vWorld;
in vec3 vNormal;
in vec4 vClip;

uniform sampler2D sceneColor;
uniform vec3 viewPos;
uniform vec3 uSunDir;
uniform float uTime;
uniform float uSeaLevel;
uniform vec3 uSkyHorizon;
uniform vec3 uSkyZenith;
uniform float uDayLight;

vec2 rippleUv(vec3 w, float t) {
    vec2 p = w.xz * 0.055;
    return vec2(sin(p.x * 2.0 + t * 1.05) + cos(p.y * 1.6 + t * 0.88) * 0.7,
                 cos(p.y * 2.0 + t * 0.92) + sin(p.x * 1.3 + t * 1.1) * 0.7)
           * 0.0065;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(viewPos - vWorld);
    float ndv = clamp(dot(N, V), 0.0, 1.0);
    float F = pow(1.0 - ndv, 3.8);

    vec2 uv = vClip.xy / vClip.w * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    vec2 duv = rippleUv(vWorld, uTime);
    vec2 suv = uv + duv + (N.xz * 0.012);

    vec3 scene = texture(sceneColor, clamp(suv, vec2(0.001), vec2(0.999))).rgb * 1.06;

    float depthHint = clamp((uSeaLevel - vWorld.y) * 0.15, 0.0, 1.0);
    vec3 shallow = vec3(0.14, 0.62, 0.78);
    vec3 deep = vec3(0.04, 0.28, 0.38);
    vec3 waterTint = mix(shallow, deep, depthHint);

    vec3 refr = mix(scene, waterTint, 0.26);
    vec3 L = normalize(-uSunDir);
    float spec = pow(max(dot(reflect(-V, N), L), 0.0), 96.0) * 0.95;
    vec3 sky = mix(uSkyHorizon, uSkyZenith, 0.48);
    vec3 refl = mix(waterTint * 0.42, sky, F) + spec;

    vec3 outc = mix(refr, refl, F * 0.72 + 0.05);
    outc *= 1.08;
    float dl = clamp(uDayLight, 0.0, 1.0);
    outc *= mix(0.28, 1.06, dl);
    float alpha = 0.38 + F * 0.12;
    FragColor = vec4(outc, alpha);
}
