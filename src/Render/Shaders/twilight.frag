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

const float PI = 3.14159265359;

vec3 twilight(float t) {
    const vec3 c0 = vec3(0.996106,0.851653,0.940566);
    const vec3 c1 = vec3(-6.529620,-0.183448,-3.940750);
    const vec3 c2 = vec3(40.899579,-7.894242,38.569228);
    const vec3 c3 = vec3(-155.212979,4.404793,-167.925730);
    const vec3 c4 = vec3(296.687222,24.084913,315.087856);
    const vec3 c5 = vec3(-261.270519,-29.995422,-266.972991);
    const vec3 c6 = vec3(85.335349,9.602600,85.227117);
    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
}

void main()
{
    vec3 sigma = texture(anisoMap, i_texCoord).xyz;
    float lambda1 = 0.5*(length(vec2(sigma.x-sigma.y, 2*sigma.z))+sigma.x+sigma.y);
    float lambda2 = (sigma.x*sigma.y - sigma.z*sigma.z)/lambda1;
    vec2 t = sigma.z == 0 && sigma.x <= sigma.y ? vec2(0, 1) : normalize(vec2(lambda1-sigma.y, sigma.z));
    t = u.anisotropy < 0 ? vec2(-t.y, t.x) : t;
    fragColor = vec4(twilight(t.x != 0 ? (atan(t.y/t.x)/PI+0.5) : 0), 1.0);
}
