#version 440

layout(location = 0) out vec4 fragColor;
layout(location = 0) in vec2 i_direction;

layout(std140, binding = 0) uniform buf {
    vec2 position;
    vec2 scale;
    vec2 dir;
    float angle;
    float skewH;
    float skewV;
    float lineX;
    float lineY;
    float lineRot;
    float lineCenterX;
    float lineCenterY;
    float lineScale;
    float hOverW;
    float tesselationLOD;
    float alignPhases;
} u;

void main()
{
    fragColor = vec4(vec2(1, -1)*normalize(i_direction), 0, u.alignPhases > 0.5 ? -1 : 1);
}
