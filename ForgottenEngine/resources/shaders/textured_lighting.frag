//glsl version 4.5
#version 450

//shader input
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec2 inTexCoord;
//output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform  SceneData{
    vec4 fog_color;// w is for exponent
    vec4 fog_distances;//x for min, y for max, zw unused.
    vec4 ambient_color;
    vec4 sunlight_direction;//w for sun power
    vec4 sunlight_color;
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D single_texture;

void main()
{
	vec3 color = texture(single_texture, inTexCoord).xyz;
	outFragColor = vec4(color, 1.0f);
}
