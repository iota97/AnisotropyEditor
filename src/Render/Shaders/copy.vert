#version 440
layout(location = 0) in vec2 a_position;
layout(location = 0) out vec2 uv;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
    uv = a_position*0.5+0.5;
    gl_Position =  vec4(a_position, 0.0f, 1.0f);
}
