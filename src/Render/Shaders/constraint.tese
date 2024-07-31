#version 440

layout (isolines, fractional_odd_spacing) in;

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


void main()
{
    vec2 p0 = gl_in[0].gl_Position.xy;
    vec2 p1 = gl_in[1].gl_Position.xy;
    float t = gl_TessCoord.x;
    vec2 position = mix(p0, p1, t);

    mat2 R = mat2(cos(u.angle), sin(u.angle), -sin(u.angle), cos(u.angle));
    mat2 S = mat2(1+u.skewH*u.skewV, u.skewV, u.skewH, 1);
    if (u.lineScale > 0) {
        mat2 R1 = mat2(cos(u.lineRot), sin(u.lineRot), -sin(u.lineRot), cos(u.lineRot));
        vec2 pos = R1*(position*vec2(u.lineScale, 1)) + 2*(vec2(u.lineX, u.lineY) - 0.5)*vec2(1, -1);
        vec2 center = 2*(vec2(u.lineCenterX, u.lineCenterY) - 0.5)*vec2(1, -1);
        gl_Position = u.MVP*vec4(R*S*((pos-center)*u.scale)+center + 2*(u.position-vec2(u.lineCenterX, u.lineCenterY))*vec2(1, -1), 0.0, 1.0f);
    } else {
        if (u.tesselationLOD > 1) position /= length(position);
        gl_Position = u.MVP*vec4(R*S*(position*u.scale) + 2*(u.position - 0.5)*vec2(1, -1), 0.0, 1.0f);
    }
    gl_Position.z -= ((u.mode != 3) ? 0.01 : 0.005)*gl_Position.w;
}
