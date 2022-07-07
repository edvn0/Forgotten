// glsl version 4.5
#version 450

// shader input
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec4 inNormals;

// output write
layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform SceneUBO
{
	vec4 fog_color; // w is for exponent
	vec4 fog_distances; // x for min, y for max, zw unused.
	vec4 ambient_color;
	vec4 sunlight_direction; // w for sun power
	vec4 sunlight_color;
}
scene_data;

void main() { 
    outFragColor = vec4(inColor.xyz + scene_data.ambient_color.xyz + inNormals.xyz, 1.0f); 
}