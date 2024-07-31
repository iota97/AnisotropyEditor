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

vec3 viridis(float t) {
    const vec3 c0 = vec3(0.274344,0.004462,0.331359);
    const vec3 c1 = vec3(0.108915,1.397291,1.388110);
    const vec3 c2 = vec3(-0.319631,0.243490,0.156419);
    const vec3 c3 = vec3(-4.629188,-5.882803,-19.646115);
    const vec3 c4 = vec3(6.181719,14.388598,57.442181);
    const vec3 c5 = vec3(4.876952,-13.955112,-66.125783);
    const vec3 c6 = vec3(-5.513165,4.709245,26.582180);
    return c0+t*(c1+t*(c2+t*(c3+t*(c4+t*(c5+t*c6)))));
}

void main()
{
    vec4 phi = (2*textureGather(anisoDir, i_texCoord, 2)-1)*PI;
    fragColor = vec4(viridis(dot(sin(phi), vec4(0.125))+0.5), 1.0);
}
