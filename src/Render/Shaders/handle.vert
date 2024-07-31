#version 440
layout(location = 0) in vec2 a_position;
layout(location = 0) out vec2 o_uv;

layout(std140, binding = 0) uniform buf {
    mat4 MVP;
    vec2 position;
    vec2 scale;
    vec2 atlasPosition;
    float angle;
    float skewH;
    float skewV;
    float mode;
    float objScale;
} u;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    mat2 R = mat2(cos(u.angle), sin(u.angle), -sin(u.angle), cos(u.angle));
    gl_Position = u.MVP*vec4(R*(a_position*u.scale) + 2*(u.position - 0.5)*vec2(1, -1), 0.0, 1.0f);
    o_uv = vec2((1.0+a_position+2*u.atlasPosition)/16.0);
}
