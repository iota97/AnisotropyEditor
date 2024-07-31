#version 440
layout(location = 0) in vec2 a_position;
layout(location = 1) in vec2 a_unused;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
};

void main()
{
    gl_Position = vec4(a_position, 0, 1);
}
