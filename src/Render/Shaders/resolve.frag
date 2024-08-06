#version 440

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

void main()
{
    vec4 col = texture(tex, uv);
    fragColor = col/col.a;
}
