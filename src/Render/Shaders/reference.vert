#version 440
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_texCoord;
layout(location = 3) in vec4 a_tangent;

layout(location = 0) out vec2 o_texCoord;

layout(binding = 2) uniform sampler2D anisoDir;
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

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    vec3 pos = a_position*vec3(u.screenWidth, 1, 1);
    o_texCoord = a_texCoord;
    gl_Position =  u.proj * u.model * vec4(pos, 1.0f);
}
