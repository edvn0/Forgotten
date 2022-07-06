#version 450

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec4 vNormal;
layout(location = 2) in vec4 vColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormals;

layout(set = 0, binding = 0) uniform  CameraBuffer{
    mat4 view;
    mat4 proj;
    mat4 viewproj;
} cameraData;

//push constants block
layout( push_constant ) uniform constants
{
    vec4 data;
    mat4 render_matrix;
} PushConstants;

void main()
{
    mat4 transformMatrix = (cameraData.viewproj * PushConstants.render_matrix);
    gl_Position = transformMatrix * vPosition;
    outColor = vColor;
}