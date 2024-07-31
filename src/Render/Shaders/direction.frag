#version 440
layout(binding = 2) uniform sampler2D anisoDir;
layout(binding = 1) uniform sampler2D anisoMap;

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec2 i_texCoord;
layout(location = 2) in mat3 i_TBN;

layout(std140, binding = 0) uniform buf {
    mat4 model;
    mat4 proj;
    vec3 albedo;
    float metallic;
    float anisotropy;
    float roughness;
    float screenWidth;
    float modelScale;
    float uvMode;
    float monteCarlo;
    float exposure;
} u;

layout(location = 0) out vec4 fragColor;

vec3 bone(float t) {
    const vec3 c0 = vec3(-0.005007,-0.003054,0.004092);
    const vec3 c1 = vec3(1.098251,0.964561,0.971829);
    const vec3 c2 = vec3(-2.688698,-0.537516,2.444353);
    const vec3 c3 = vec3(12.667310,-0.657473,-8.158684);
    const vec3 c4 = vec3(-27.183124,8.398806,10.182004);
    const vec3 c5 = vec3(26.505377,-12.576925,-5.329155);
    const vec3 c6 = vec3(-9.395265,5.416416,0.883918);
    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
}

void main()
{
    vec3 sigma = texture(anisoMap, i_texCoord).xyz;
    float lambda1 = 0.5*(length(vec2(sigma.x-sigma.y, 2*sigma.z))+sigma.x+sigma.y);
    float lambda2 = (sigma.x*sigma.y - sigma.z*sigma.z)/lambda1;
    vec2 t = sigma.z == 0 && sigma.x <= sigma.y ? vec2(0, 1) : normalize(vec2(lambda1-sigma.y, sigma.z));
    vec3 tangent = i_TBN*vec3(u.anisotropy < 0 ? vec2(-t.y, t.x) : t, 0.0);

    float res = 0;
    float level = 128*u.modelScale;
    for (float i = 0; i < 32; i++) {
        vec3 samplePoint = i_position+tangent*(i-15.5)/level;
        res += fract(sin(dot(floor(samplePoint*level)/level, vec3(12.9898, 78.233, 545.45))) * 43758.5453);
    }
    res = clamp(2*(res/32-0.5)+0.5, 0, 1);
    fragColor = vec4(bone(res), 1.0);
}
