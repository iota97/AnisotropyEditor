#version 440
layout(binding = 1) uniform sampler2D anisoMap;
layout(binding = 3) uniform samplerCube cubeMap;

layout(location = 0) in vec3 i_worldPos;
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
    float flag;
    float uvMode;
    float monteCarlo;
    float exposure;
} u;

layout(location = 0) out vec4 fragColor;

const float PI = 3.14159265359;

vec4 textureNoFiltering(sampler2D tex, vec2 coord)
{
    vec2 texSize = textureSize(tex, 0);
    return textureLod(tex, (round(coord*texSize-0.5)+0.5)/texSize, 0);
}

uint hash(uint x)
{
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

float floatConstruct(uint m)
{
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;
}

float random(float x)
{
    return floatConstruct(hash(floatBitsToUint(x)));
}

vec3 sampleGGXVNDF(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1,0,0);
    vec3 T2 = cross(Vh, T1);
    float r = sqrt(U1);
    float phi = 2.0 * PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s)*sqrt(1.0 - t1*t1) + s*t2;
    vec3 Nh = t1*T1 + t2*T2 + sqrt(max(0.0, 1.0 - t1*t1 - t2*t2))*Vh;
    vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 sigma = textureNoFiltering(anisoMap, i_texCoord).xyz;
    float lambda1 = 0.5*(length(vec2(sigma.x-sigma.y, 2*sigma.z))+sigma.x+sigma.y);
    float lambda2 = (sigma.x*sigma.y - sigma.z*sigma.z)/lambda1;
    vec2 t = sigma.z == 0 && sigma.x <= sigma.y ? vec2(0, 1) : normalize(vec2(lambda1-sigma.y, sigma.z));
    vec2 b = vec2(-t.y, t.x);

    vec3 view = normalize(vec3(0, 0, 4)-i_worldPos);
    mat3 ellipseToWorld = i_TBN * mat3(vec3(t, 0), vec3(b, 0), vec3(0, 0, 1));
    vec3 wo = view * ellipseToWorld;
    vec3 monteCarloIBL = vec3(0.0);
    float rand = random(u.monteCarlo + i_texCoord.x*(i_texCoord.y+1));

    vec2 alpha = sqrt(2*vec2(lambda1, lambda2));
    vec3 lvt = vec3(alpha.x*wo.x, alpha.y*wo.y, wo.z);
    lvt *= lvt;
    float LV = (-1 + sqrt(1 + (lvt.x+lvt.y)/lvt.z))*0.5;
    float G1V = 1.0/(1.0+LV);

    const uint N = 16;
    vec3 Ftot = vec3(0);
    for (uint i = 0; i < N; i++) {
        float tmp = random(rand);
        rand = random(tmp);
        vec3 wm = sampleGGXVNDF(wo, alpha.x, alpha.y, tmp, rand);
        vec3 wi = reflect(-wo, wm);
        vec3 Li = textureLod(cubeMap, ellipseToWorld*wi, 0).rgb;

        vec3 lvt = vec3(alpha.x*wi.x, alpha.y*wi.y, wi.z);
        lvt *= lvt;
        float LL = (-1 + sqrt(1 + (lvt.x+lvt.y)/lvt.z))*0.5;
        float G2 = 1.0/(1.0+LV+LL);
        vec3 F = fresnelSchlick(dot(wo, wm), mix(vec3(0.04), u.albedo, u.metallic));
        Ftot += F;
        monteCarloIBL += F*Li*G2/G1V;
    }

    vec3 res = monteCarloIBL/N;
    res += (1-Ftot/N)*u.albedo/PI * textureLod(cubeMap, reflect(-view, i_TBN[2]), log2(textureSize(cubeMap, 0).x)).rgb;

    // Tonemapping
    float luma = dot(res, vec3(0.212671, 0.715160, 0.072169));
    res *= (1+luma*u.exposure*u.exposure)/(1+luma);
    fragColor = pow(vec4(res, 1.0), vec4(1.0/2.2));
}
