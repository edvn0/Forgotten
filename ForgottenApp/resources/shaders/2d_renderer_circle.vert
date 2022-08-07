#version 430 core

layout(location = 0) in vec3 a_WorldPosition;
layout(location = 1) in float a_Thickness;
layout(location = 2) in vec2 a_LocalPosition;
layout(location = 3) in vec4 a_Color;

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
    vec2 LocalPosition;
    float Thickness;
    vec4 Color;
};

layout (location = 0) out VertexOutput Output;

void main()
{
    Output.LocalPosition = a_LocalPosition;
    Output.Thickness = a_Thickness;
    Output.Color = a_Color;
    gl_Position = u_ViewProjection * u_Renderer.Transform * vec4(a_WorldPosition, 1.0);
}
