#version 440
layout(binding = 1) uniform sampler2D anisoMap;
layout(binding = 5) uniform samplerCube cubeMap;

layout(location = 0) in vec3 i_worldPos;
layout(location = 1) in vec2 i_texCoord;
layout(location = 2) in mat3 i_TBN;

layout(std140, binding = 0) uniform buf {
    mat4 model;
    mat4 proj;
    vec3 albedo;
    float metallic;
    float anisotropy;
    float unused1;
    float unused2;
    float unused3;
    float sampleCount;
    float unused5;
    float exposure;
} u;

layout(location = 0) out vec4 fragColor;

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float lodFromRoughness(float rough, samplerCube tex)
{
    return log2(textureSize(tex, 0).x)*rough;
}

void main()
{
    // Hard-coded given our usecase
    vec3 view = normalize(vec3(0, 0, 4)-i_worldPos);

    // Eigenvector decomposition
    vec3 sigma = texture(anisoMap, i_texCoord).xyz;
    float lambda1 = 0.5*(length(vec2(sigma.x-sigma.y, 2*sigma.z))+sigma.x+sigma.y);
    float lambda2 = (sigma.x*sigma.y - sigma.z*sigma.z)/lambda1;
    vec2 t = sigma.z == 0 && sigma.x <= sigma.y ? vec2(0, 1) : normalize(vec2(lambda1-sigma.y, sigma.z));
    float rough = sqrt(0.5*(sqrt(2*lambda1)+sqrt(2*lambda2)));
    float anisotropy = (sqrt(lambda1)-sqrt(lambda2))/(sqrt(lambda1)+sqrt(lambda2));

    // Bent normal
    vec3 tangent = i_TBN*normalize(vec3(t, 0));
    vec3 normal = normalize(i_TBN[2]);
    vec3 bitangent = normalize(cross(normal, tangent));

    vec3 grainDirWs = anisotropy >= 0 ? bitangent : tangent;
    float stretch = abs(anisotropy) * clamp(1.5*sqrt(rough), 0, 1);
    vec3 B = cross(grainDirWs, view);
    vec3 grainNormal = cross(B, grainDirWs);
    vec3 iblN = normalize(mix(normal, grainNormal, stretch));
    float roughIBL = rough*clamp(1.2-abs(anisotropy), 0, 1);
    vec3 res = textureLod(cubeMap, reflect(-view, iblN), lodFromRoughness(roughIBL, cubeMap)).rgb;

    // Pesce and Iwanicki 2015
    float NoV = dot(view, i_TBN[2]);
    vec3 F = fresnelSchlick(NoV, mix(vec3(0.04), u.albedo, u.metallic));
    float bias = pow(2., -7.*(NoV+4.*rough*rough));
    float scale = 1. - bias - rough*rough*max(bias, min(rough, 0.739+0.323*NoV)-0.434);
    res *= F * scale + bias;
    res += (1-F)*u.albedo/PI * textureLod(cubeMap, reflect(-view, normal), log2(textureSize(cubeMap, 0).x)).rgb;

    // Tonemapping
    float luma = dot(res, vec3(0.212671, 0.715160, 0.072169));
    res *= (1+luma*u.exposure*u.exposure)/(1+luma);
    fragColor = vec4(pow(res, vec3(1.0/2.2)), 1.0);
}
