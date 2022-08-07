#version 430 core

layout(location = 0) out vec4 color;

struct VertexOutput
{
    vec2 LocalPosition;
    float Thickness;
    vec4 Color;
};

layout (location = 0) in VertexOutput Input;

void main()
{
    float fade = 0.01;
    float dist = sqrt(dot(Input.LocalPosition, Input.LocalPosition));
    if (dist > 1.0 || dist < 1.0 - Input.Thickness - fade)
    discard;

    float alpha = 1.0 - smoothstep(1.0f - fade, 1.0f, dist);
    alpha *= smoothstep(1.0 - Input.Thickness - fade, 1.0 - Input.Thickness, dist);
    color = Input.Color;
    color.a = alpha;
}
