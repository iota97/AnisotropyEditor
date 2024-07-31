#version 440
layout(binding = 4) uniform sampler2D reference;

layout(location = 0) in vec2 i_texCoord;

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

void main()
{
    fragColor = texture(reference, i_texCoord);
}
