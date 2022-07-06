#version 450

layout(location = 0) in vec4 vPosition;
layout(location = 1) in vec4 vNormal;
layout(location = 2) in vec4 vColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outNormals;

// push constants block
layout(push_constant) uniform constants
{
	vec4 data;
	mat4 render_matrix;
}
PushConstants;

void main()
{
	gl_Position = PushConstants.render_matrix * vPosition;
	outColor = vColor;
}