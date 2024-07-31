#version 440

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 MVP;
    vec2 position;
    vec2 scale;
    vec2 dir;
    float angle;
    float skewH;
    float skewV;
    float mode;
    float objScale;
    float lineX;
    float lineY;
    float lineRot;
    float lineCenterX;
    float lineCenterY;
    float lineScale;
    float tesselationLOD;
} u;

const float PI = 3.14159265359;

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    vec2 dir = normalize(u.dir);

    if (u.mode == 1) {
        fragColor = vec4(1);
    } else if (u.mode == 3) {
        fragColor = vec4(0.75, 0.75, 0.75, 1.0);
    } else {
        fragColor = vec4(hsv2rgb(vec3(mod(atan(dir.y, dir.x), 1), u.mode == 0 ? 0.5 : 1, 1)), 1);
    }
}
