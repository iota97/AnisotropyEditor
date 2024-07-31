#version 440

layout(location = 0) out vec4 fragColor;
layout(location = 0) in vec2 i_uv;

layout(binding = 1) uniform sampler2D atlas;
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
} u;

const float PI = 3.14159265359;

void main()
{
    fragColor = texture(atlas, i_uv);
}
