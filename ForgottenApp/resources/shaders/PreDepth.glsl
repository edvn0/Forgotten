// Pre-depth shader

#version 450 core
#pragma stage : vert

layout(std140, binding = 0) uniform Camera
{
	mat4 ViewProjectionMatrix;
	mat4 InverseViewProjectionMatrix;
	mat4 ProjectionMatrix;
	mat4 InverseProjectionMatrix;
	mat4 ViewMatrix;
	mat4 InverseViewMatrix;
	vec2 NDCToViewMul;
	vec2 NDCToViewAdd;
	vec2 DepthUnpackConsts;
	vec2 CameraTanHalfFOV;
} u_Camera;

layout (std140, binding = 1) uniform ShadowData
{
	mat4 DirLightMatrices[4];
} u_DirShadow;

struct DirectionalLight
{
	vec3 Direction;
	float ShadowAmount;
	vec3 Radiance;
	float Multiplier;
};

layout(std140, binding = 2) uniform SceneData
{
	DirectionalLight DirectionalLights;
	vec3 CameraPosition; // Offset = 32
	float EnvironmentMapIntensity;
} u_Scene;


layout(std140, binding = 3) uniform RendererData
{
	uniform vec4 CascadeSplits;
	uniform int TilesCountX;
	uniform bool ShowCascades;
	uniform bool SoftShadows;
	uniform float LightSize;
	uniform float MaxShadowDistance;
	uniform float ShadowFade;
	uniform bool CascadeFading;
	uniform float CascadeTransitionFade;
	uniform bool ShowLightComplexity;
} u_RendererData;

layout(std140, binding = 17) uniform ScreenData
{
    vec2 InvFullResolution;
    vec2 FullResolution;
	vec2 InvHalfResolution;
    vec2 HalfResolution;
} u_ScreenData;


layout(std140, binding = 18) uniform HBAOData
{
	vec4	PerspectiveInfo;
	vec2    InvQuarterResolution;
	float   RadiusToScreen;
	float   NegInvR2;
	float   NDotVBias;
	float   AOMultiplier;
	float   PowExponent;
	bool	IsOrtho;
	vec4    Float2Offsets[16];
	vec4    Jitters[16];
	vec3	Padding;
	float	ShadowTolerance;
} u_HBAO;

struct PointLight
{
	vec3 Position;
	float Multiplier;
	vec3 Radiance;
	float MinRadius;
	float Radius;
	float Falloff;
	float LightSize;
	bool CastsShadows;
};

layout(std140, binding = 4) uniform PointLightData
{
	uint LightCount;
	PointLight Lights[1000];
} u_PointLights;

struct SpotLight
{
	vec3 Position;
	float Multiplier;
	vec3 Direction;
	float AngleAttenuation;
	vec3 Radiance;
	float Range;
	float Angle;
	float Falloff;
	bool SoftShadows;
	bool CastsShadows;
};

layout(std140, binding = 19) uniform SpotLightData
{
	uint LightCount;
	SpotLight Lights[1024];
} u_SpotLights;

layout(std140, binding = 20) uniform SpotShadowData
{
	mat4 Mats[1024];
} u_SpotLightMatrices;

layout(std430, binding = 14) buffer VisiblePointLightIndicesBuffer
{
	int Indices[];
} s_VisiblePointLightIndicesBuffer;

layout(std430, binding = 23) writeonly buffer VisibleSpotLightIndicesBuffer
{
	int Indices[];
} s_VisibleSpotLightIndicesBuffer;



// Vertex buffer
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

// Transform buffer
layout(location = 5) in vec4 a_MRow0;
layout(location = 6) in vec4 a_MRow1;
layout(location = 7) in vec4 a_MRow2;

// Make sure both shaders compute the exact same answer(PBR shader).
// We need to have the same exact calculations to produce the gl_Position value (eg. matrix multiplications).
precise invariant gl_Position;

void main()
{
	mat4 transform = mat4(
		vec4(a_MRow0.x, a_MRow1.x, a_MRow2.x, 0.0),
		vec4(a_MRow0.y, a_MRow1.y, a_MRow2.y, 0.0),
		vec4(a_MRow0.z, a_MRow1.z, a_MRow2.z, 0.0),
		vec4(a_MRow0.w, a_MRow1.w, a_MRow2.w, 1.0)
	);

	vec4 worldPosition = transform * vec4(a_Position, 1.0);

    gl_Position = u_Camera.ViewProjectionMatrix * worldPosition;
}

#version 450 core
#pragma stage : frag

void main()
{
}