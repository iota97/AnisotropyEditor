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
    float unused0;
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

void main()
{
    // Hard-coded given our usecase
    vec3 view = normalize(vec3(0, 0, 4)-i_worldPos);

    // Eigenvector decomposition
    vec3 sigma = texture(anisoMap, i_texCoord).xyz;
    float lambda1 = 0.5*(length(vec2(sigma.x-sigma.y, 2*sigma.z))+sigma.x+sigma.y);
    float lambda2 = (sigma.x*sigma.y - sigma.z*sigma.z)/lambda1;
    vec2 t = sigma.z == 0 && sigma.x <= sigma.y ? vec2(0, 1) : normalize(vec2(lambda1-sigma.y, sigma.z));
    vec2 b = vec2(-t.y, t.x);

    // Ellipse
    mat3 ellipseToWorld = i_TBN * mat3(vec3(t, 0), vec3(b, 0), vec3(0, 0, 1));
    vec3 wo = view*ellipseToWorld;
    vec2 elp = vec2(sqrt(lambda1), sqrt(lambda2));

    // Solve for the roots
    float radius_x = elp.x;
    float radius_y = elp.y;
    vec2 wo2 = wo.xy*wo.xy;
    float r2x = radius_x*radius_x;
    float r2y = radius_y*radius_y;
    float p0 = 2.*wo.x*wo.y*radius_x*radius_y;
    float p1 = wo2.x*(r2x*r2y + r2y) - wo2.y*(r2x*r2y + r2x) + r2x -r2y;
    float p2 = -p0*(r2y + 1.);
    float p3 = p0*p2*(r2x + 1.);
    float root0 = p2 == 0. ? step(0., p1)*PI/2. : atan((p1+sqrt(p1*p1-p3))/p2);
    float root1 = p2 == 0. ? root0+PI/2. : atan((p1-sqrt(p1*p1-p3))/p2);

    // Get the major axis extrema
    vec2 a0 = elp.xy*vec2(cos(root1), sin(root1));
    vec3 wm0 = normalize(vec3(-a0, 1));
    vec3 wm1 = normalize(vec3(a0, 1));

    // Avoid below hemisphere samples
    p0 = wm1.x*wo.x + wm1.y*wo.y;
    p1 = wo.z*(1-wm1.z*wm1.z);
    p2 = sqrt(p0*p0 + wo.z*p1);
    float t0 = max(0.5+wm1.z*(p0-p2)/(2*p1), 0);
    float t1 = min(0.5+wm1.z*(p0+p2)/(2*p1), 1);
    vec3 tmp = wm0;
    wm0 = normalize(mix(wm0, wm1, t0));
    wm1 = normalize(mix(tmp, wm1, t1));

    // Update the minor axis extrema
    float angleDelta = (t0 > 0 ? t0 : t1-1)*(root0-root1);
    float min0t = root0+angleDelta;
    float min1t = root0-angleDelta+PI;
    vec2 min0Slope = elp.xy*vec2(cos(min0t), sin(min0t));
    vec2 min1Slope = elp.xy*vec2(cos(min1t), sin(min1t));
    vec3 min0Normal = normalize(vec3(-min0Slope, 1));
    vec3 min1Normal = normalize(vec3(-min1Slope, 1));

    // Shift the major axis
    float minLength = length(reflect(-wo, min0Normal)-reflect(-wo, min1Normal));
    float maxLength = length(reflect(-wo, wm0)-reflect(-wo, wm1));
    float rateo = (maxLength-minLength)/maxLength;
    vec3 wc = normalize(wm0+wm1);
    wm0 = ellipseToWorld*normalize(wc + rateo*(wm0-wc));
    wm1 = ellipseToWorld*normalize(wc + rateo*(wm1-wc));

    // LOD selecion
    const float N = u.sampleCount;
    float sampleWidth = max(minLength, 2.3*maxLength/N)/2;
    float level = log2(float(textureSize(cubeMap, 0).x))*sqrt(sampleWidth);

    // Sample the environment map
    vec3 res = vec3(0.0);
    for (float i = 0; i < N; i++) {
        vec3 wm = normalize(mix(wm0, wm1, i/(N-1)));
        vec3 wi = reflect(-view, wm);
        res += textureLod(cubeMap, wi, level).rgb;
    }
    res /= N;

    // Pesce and Iwanicki 2015
    float rough = sqrt(0.5*(sqrt(2*lambda1)+sqrt(2*lambda2)));
    float NoV = dot(view, i_TBN[2]);
    vec3 F = fresnelSchlick(NoV, mix(vec3(0.04), u.albedo, u.metallic));
    float bias = pow(2., -7.*(NoV+4.*rough*rough));
    float scale = 1. - bias - rough*rough*max(bias, min(rough, 0.739+0.323*NoV)-0.434);
    res *= F * scale + bias;

    // Use the last LOD rather than computing the radiance map, for now.
    res += (1-F)*u.albedo/PI * textureLod(cubeMap, wc, log2(textureSize(cubeMap, 0).x)).rgb;

    // Tonemapping
    float luma = dot(res, vec3(0.212671, 0.715160, 0.072169));
    res *= (1+luma*u.exposure*u.exposure)/(1+luma);
    fragColor = vec4(pow(res, vec3(1.0/2.2)), 1.0);
}
