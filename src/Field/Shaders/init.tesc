#version 440
layout(vertices = 2) out;

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
} u;

void main()
{
    if (gl_InvocationID == 0) {
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = u.tesselationLOD;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
