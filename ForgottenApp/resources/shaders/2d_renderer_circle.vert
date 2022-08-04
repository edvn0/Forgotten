#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;

layout (std140, binding = 0) uniform Camera
{
    mat4 u_ViewProjection;
};

layout (push_constant) uniform Transform
{
    mat4 Transform;
} u_Renderer;

struct VertexOutput
{
    vec4 Color;
    vec2 TexCoord;
    float TilingFactor;
};

layout (location = 0) out VertexOutput Output;
layout (location = 5) out flat float TexIndex;

void main()
{
    Output.Color = a_Color;
    Output.TexCoord = a_TexCoord;
    TexIndex = a_TexIndex;
    Output.TilingFactor = a_TilingFactor;
    gl_Position = u_ViewProjection * u_Renderer.Transform * vec4(a_Position, 1.0);
}
