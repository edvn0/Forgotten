
#version 450 core
#pragma stage : frag

layout(location = 0) out vec4 color;

struct VertexOutput
{
    vec4 Color;
    vec2 TexCoord;
};

layout (location = 0) in VertexOutput Input;
layout (location = 5) in flat float TexIndex;

layout (binding = 1) uniform sampler2D u_FontAtlases[32];

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

/* For 2D
float ScreenPxRange()
{
	float pixRange = 2.0f;
	float geoSize = 72.0f;
	return geoSize / 32.0f * pixRange;
}
*/

float ScreenPxRange()
{
    float pxRange = 2.0f;
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(u_FontAtlases[int(TexIndex)], 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(Input.TexCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

void main()
{
    vec4 bgColor = vec4(Input.Color.rgb, 0.0); // TODO(Yan): outlines
    vec4 fgColor = Input.Color;

    vec3 msd = texture(u_FontAtlases[int(TexIndex)], Input.TexCoord).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = ScreenPxRange() * (sd - 0.5f);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    color = mix(bgColor, fgColor, opacity);
}
