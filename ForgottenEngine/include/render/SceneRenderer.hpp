//
// Created by Edwin Carlsson on 2022-08-10.
//

#pragma once

#include "Reference.hpp"
#include "render/ComputePipeline.hpp"
#include "render/Framebuffer.hpp"
#include "render/Image.hpp"
#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Pipeline.hpp"
#include "render/RenderCommandBuffer.hpp"
#include "render/RenderPass.hpp"
#include "render/Renderer2D.hpp"
#include "render/RendererContext.hpp"
#include "render/Shader.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"

#include <glm/glm.hpp>

namespace ForgottenEngine {

	class SceneRenderer : public ReferenceCounted {
	public:
		SceneRenderer() { init(); }
		void init();
		void init_materials();

		Reference<Pipeline> GetFinalPipeline();
		Reference<RenderPass> GetFinalRenderPass();
		Reference<RenderPass> GetCompositeRenderPass();
		Reference<RenderPass> GetExternalCompositeRenderPass();
		Reference<Image2D> GetFinalPassImage();

	private:
		Reference<RenderCommandBuffer> m_CommandBuffer;

		Reference<Renderer2D> m_Renderer2D;

		struct PointLight {
			glm::vec3 from;
			glm::vec3 dir;
		};

		struct UBCamera {
			// projection with near and far inverted
			glm::mat4 ViewProjection;
			glm::mat4 InverseViewProjection;
			glm::mat4 Projection;
			glm::mat4 InverseProjection;
			glm::mat4 View;
			glm::mat4 InverseView;
			glm::vec2 NDCToViewMul;
			glm::vec2 NDCToViewAdd;
			glm::vec2 DepthUnpackConsts;
			glm::vec2 CameraTanHalfFOV;
		} CameraDataUB;

		struct UBHBAOData {
			glm::vec4 PerspectiveInfo;
			glm::vec2 InvQuarterResolution;
			float RadiusToScreen;
			float NegInvR2;

			float NDotVBias;
			float AOMultiplier;
			float PowExponent;
			bool IsOrtho;
			char Padding0[3] { 0, 0, 0 };

			glm::vec4 Float2Offsets[16];
			glm::vec4 Jitters[16];

			glm::vec3 Padding;
			float ShadowTolerance;
		} HBAODataUB;

		struct CBGTAOData {
			glm::vec2 NDCToViewMul_x_PixelSize;
			float EffectRadius = 0.5f;
			float EffectFalloffRange = 0.62f;

			float RadiusMultiplier = 1.46f;
			float FinalValuePower = 2.2f;
			float DenoiseBlurBeta = 1.2f;
			bool HalfRes = false;
			char Padding0[3] { 0, 0, 0 };

			float SampleDistributionPower = 2.0f;
			float ThinOccluderCompensation = 0.0f;
			float DepthMIPSamplingOffset = 3.3f;
			int NoiseIndex = 0;

			glm::vec2 HZBUVFactor;
			float ShadowTolerance;
			float Padding;
		} GTAODataCB;

		struct UBScreenData {
			glm::vec2 InvFullResolution;
			glm::vec2 FullResolution;
			glm::vec2 InvHalfResolution;
			glm::vec2 HalfResolution;
		} ScreenDataUB;

		struct UBShadow {
			glm::mat4 ViewProjection[4];
		} ShadowData;

		struct DirLight {
			glm::vec3 Direction;
			float ShadowAmount;
			glm::vec3 Radiance;
			float Intensity;
		};

		struct UBPointLights {
			uint32_t Count { 0 };
			glm::vec3 Padding {};
			PointLight PointLights[1024] {};
		} PointLightsUB;

		struct UBSpotLights {
			uint32_t Count { 0 };
			glm::vec3 Padding {};
			PointLight SpotLights[1000] {};
		} SpotLightUB;

		struct UBSpotShadowData {
			glm::mat4 ShadowMatrices[1000] {};
		} SpotShadowDataUB;

		struct UBScene {
			DirLight Lights;
			glm::vec3 CameraPosition;
			float EnvironmentMapIntensity = 1.0f;
		} SceneDataUB;

		struct UBRendererData {
			glm::vec4 CascadeSplits;
			uint32_t TilesCountX { 0 };
			bool ShowCascades = false;
			char Padding0[3] = { 0, 0, 0 }; // Bools are 4-bytes in GLSL
			bool SoftShadows = true;
			char Padding1[3] = { 0, 0, 0 };
			float Range = 0.5f;
			float MaxShadowDistance = 200.0f;
			float ShadowFade = 1.0f;
			bool CascadeFading = true;
			char Padding2[3] = { 0, 0, 0 };
			float CascadeTransitionFade = 1.0f;
			bool ShowLightComplexity = false;
			char Padding3[3] = { 0, 0, 0 };
		} RendererDataUB;

		// GTAO
		Reference<Image2D> m_GTAOOutputImage;
		Reference<Image2D> m_GTAODenoiseImage;
		// Points to m_GTAOOutputImage or m_GTAODenoiseImage!
		Reference<Image2D> m_GTAOFinalImage; // TODO: WeakRef!
		Reference<Image2D> m_GTAOEdgesOutputImage;
		Reference<ComputePipeline> m_GTAOPipeline;
		Reference<Material> m_GTAOMaterial;
		glm::uvec3 m_GTAOWorkGroups { 1 };
		Reference<ComputePipeline> m_GTAODenoisePipeline;
		Reference<Material> m_GTAODenoiseMaterial[2]; // Ping, Pong
		Reference<Material> m_AOCompositeMaterial;
		glm::uvec3 m_GTAODenoiseWorkGroups { 1 };
		Reference<Pipeline> m_AOCompositePipeline;
		Reference<RenderPass> m_AOCompositeRenderPass;

		// HBAO
		glm::uvec3 m_HBAOWorkGroups { 1 };
		Reference<Material> m_DeinterleavingMaterial;
		Reference<Material> m_ReinterleavingMaterial;
		Reference<Material> m_HBAOBlurMaterials[2];
		Reference<Material> m_HBAOMaterial;
		Reference<Pipeline> m_DeinterleavingPipelines[2];
		Reference<Pipeline> m_ReinterleavingPipeline;
		Reference<Pipeline> m_HBAOBlurPipelines[2];

		Reference<ComputePipeline> m_HBAOPipeline;
		Reference<Image2D> m_HBAOOutputImage;

		Reference<Shader> m_CompositeShader;

		// Shadows
		Reference<Pipeline> m_SpotShadowPassPipeline;
		Reference<Pipeline> m_SpotShadowPassAnimPipeline;
		Reference<Material> m_SpotShadowPassMaterial;

		glm::uvec3 m_LightCullingWorkGroups;

		Reference<UniformBufferSet> m_UniformBufferSet;
		Reference<StorageBufferSet> m_StorageBufferSet;

		float LightDistance = 0.1f;
		float CascadeSplitLambda = 0.92f;
		glm::vec4 CascadeSplits;
		float CascadeFarPlaneOffset = 50.0f, CascadeNearPlaneOffset = -50.0f;

		// SSR
		Reference<Pipeline> m_SSRCompositePipeline;
		Reference<ComputePipeline> m_SSRPipeline;
		Reference<ComputePipeline> m_HierarchicalDepthPipeline;
		Reference<ComputePipeline> m_GaussianBlurPipeline;
		Reference<ComputePipeline> m_PreIntegrationPipeline;
		Reference<ComputePipeline> m_SSRUpscalePipeline;
		Reference<Image2D> m_SSRImage;
		Reference<Texture2D> m_HierarchicalDepthTexture;
		Reference<Texture2D> m_PreConvolutedTexture;
		Reference<Texture2D> m_VisibilityTexture;
		Reference<Material> m_SSRCompositeMaterial;
		Reference<Material> m_SSRMaterial;
		Reference<Material> m_GaussianBlurMaterial;
		Reference<Material> m_HierarchicalDepthMaterial;
		Reference<Material> m_PreIntegrationMaterial;
		glm::uvec3 m_SSRWorkGroups { 1 };

		glm::vec2 FocusPoint = { 0.5f, 0.5f };

		Reference<Material> m_CompositeMaterial;
		Reference<Material> m_LightCullingMaterial;

		Reference<Pipeline> m_GeometryPipeline;
		Reference<Pipeline> m_TransparentGeometryPipeline;
		Reference<Pipeline> m_GeometryPipelineAnim;

		Reference<Pipeline> m_SelectedGeometryPipeline;
		Reference<Pipeline> m_SelectedGeometryPipelineAnim;
		Reference<Material> m_SelectedGeometryMaterial;
		Reference<Material> m_SelectedGeometryMaterialAnim;

		Reference<Pipeline> m_GeometryWireframePipeline;
		Reference<Pipeline> m_GeometryWireframePipelineAnim;
		Reference<Pipeline> m_GeometryWireframeOnTopPipeline;
		Reference<Pipeline> m_GeometryWireframeOnTopPipelineAnim;
		Reference<Material> m_WireframeMaterial;

		Reference<Pipeline> m_PreDepthPipeline;
		Reference<Pipeline> m_PreDepthTransparentPipeline;
		Reference<Pipeline> m_PreDepthPipelineAnim;
		Reference<Material> m_PreDepthMaterial;

		Reference<Pipeline> m_CompositePipeline;

		Reference<Pipeline> m_ShadowPassPipelines[4];
		Reference<Pipeline> m_ShadowPassPipelinesAnim[4];

		Reference<Material> m_ShadowPassMaterial;

		Reference<Pipeline> m_SkyboxPipeline;
		Reference<Material> m_SkyboxMaterial;

		Reference<Pipeline> m_DOFPipeline;
		Reference<Material> m_DOFMaterial;

		Reference<ComputePipeline> m_LightCullingPipeline;

		// Jump Flood Pass
		Reference<Pipeline> m_JumpFloodInitPipeline;
		Reference<Pipeline> m_JumpFloodPassPipeline[2];
		Reference<Pipeline> m_JumpFloodCompositePipeline;
		Reference<Material> m_JumpFloodInitMaterial, m_JumpFloodPassMaterial[2];
		Reference<Material> m_JumpFloodCompositeMaterial;

		// Bloom compute
		uint32_t m_BloomComputeWorkgroupSize = 4;
		Reference<ComputePipeline> m_BloomComputePipeline;
		Reference<Texture2D> m_BloomComputeTextures[3];
		Reference<Material> m_BloomComputeMaterial;

		struct TransformVertexData {
			glm::vec4 MRow[3];
		};

		struct TransformBuffer {
			Reference<VertexBuffer> Buffer;
			TransformVertexData* Data = nullptr;
		};

		std::vector<Reference<Framebuffer>> m_TempFramebuffers;

		Reference<RenderPass> m_ExternalCompositeRenderPass;

		// Grid
		Reference<Pipeline> m_GridPipeline;
		Reference<Material> m_GridMaterial;

		Reference<Material> m_OutlineMaterial;
		Reference<Material> m_SimpleColliderMaterial;
		Reference<Material> m_ComplexColliderMaterial;

		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
		float m_InvViewportWidth = 0.f, m_InvViewportHeight = 0.f;
		bool m_NeedsResize = false;
		bool m_Active = false;
		bool m_ResourcesCreated = false;

		float m_LineWidth = 2.0f;

		Reference<Texture2D> m_BloomDirtTexture;

		Reference<Image2D> m_ReadBackImage;
		glm::vec4* m_ReadBackBuffer = nullptr;

		float m_Opacity = 1.0f;

		struct GPUTimeQueries {
			uint32_t DirShadowMapPassQuery = 0;
			uint32_t SpotShadowMapPassQuery = 0;
			uint32_t DepthPrePassQuery = 0;
			uint32_t HierarchicalDepthQuery = 0;
			uint32_t PreIntegrationQuery = 0;
			uint32_t LightCullingPassQuery = 0;
			uint32_t GeometryPassQuery = 0;
			uint32_t PreConvolutionQuery = 0;
			uint32_t HBAOPassQuery = 0;
			uint32_t GTAOPassQuery = 0;
			uint32_t GTAODenoisePassQuery = 0;
			uint32_t AOCompositePassQuery = 0;
			uint32_t SSRQuery = 0;
			uint32_t SSRCompositeQuery = 0;
			uint32_t BloomComputePassQuery = 0;
			uint32_t JumpFloodPassQuery = 0;
			uint32_t CompositePassQuery = 0;
		} m_GPUTimeQueries;

		friend class SceneRendererPanel;
	};

} // namespace ForgottenEngine