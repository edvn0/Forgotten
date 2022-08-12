#pragma once

#include "RenderPass.hpp"
#include "StorageBufferSet.hpp"
#include "render/ComputePipeline.hpp"
#include "render/Mesh.hpp"
#include "render/RenderCommandBuffer.hpp"
#include "render/UniformBufferSet.hpp"
#include "scene/Components.hpp"
#include "scene/Scene.hpp"

namespace ForgottenEngine {

	namespace ShaderDef {

		enum class AOMethod {
			None = 0,
			GTAO = BIT(1),
			HBAO = BIT(2),
			All = GTAO | HBAO,
		};

		constexpr static std::underlying_type_t<AOMethod> GetMethodIndex(const AOMethod method)
		{
			switch (method) {
			case AOMethod::None:
				return 0;
			case AOMethod::GTAO:
				return 1;
			case AOMethod::HBAO:
				return 2;
			case AOMethod::All:
				return 3;
			}
			return 0;
		}

		constexpr static ShaderDef::AOMethod ROMETHODS[4] = { AOMethod::None, AOMethod::GTAO, AOMethod::HBAO, AOMethod::All };

		constexpr static AOMethod GetAOMethod(const bool gtao_enabled, const bool hba_enabled)
		{
			if (gtao_enabled && hba_enabled)
				return AOMethod::All;
			else if (gtao_enabled)
				return AOMethod::GTAO;
			else if (hba_enabled)
				return AOMethod::HBAO;
			else
				return AOMethod::None;
		}
	}; // namespace ShaderDef

	struct SceneRendererOptions {
		bool ShowGrid = true;
		bool ShowSelectedInWireframe = false;

		enum class PhysicsColliderView {
			SelectedEntity = 0,
			All = 1
		};

		bool ShowPhysicsColliders = false;
		PhysicsColliderView PhysicsColliderMode = PhysicsColliderView::SelectedEntity;
		bool ShowPhysicsCollidersOnTop = false;
		glm::vec4 SimplePhysicsCollidersColor = glm::vec4 { 0.2f, 1.0f, 0.2f, 1.0f };
		glm::vec4 ComplexPhysicsCollidersColor = glm::vec4 { 0.5f, 0.5f, 1.0f, 1.0f };

		// General AO
		float AOShadowTolerance = 0.15f;

		// HBAO
		bool EnableHBAO = false;
		float HBAOIntensity = 1.5f;
		float HBAORadius = 1.0f;
		float HBAOBias = 0.35f;
		float HBAOBlurSharpness = 1.0f;

		// GTAO
		bool EnableGTAO = true;
		bool GTAOBentNormals = false;
		int GTAODenoisePasses = 4;

		// SSR
		bool EnableSSR = false;
		ShaderDef::AOMethod ReflectionOcclusionMethod = ShaderDef::AOMethod::None;
	};

	struct SSROptionsUB {
		// SSR
		glm::vec2 HZBUvFactor;
		glm::vec2 FadeIn = { 0.1f, 0.15f };
		float Brightness = 0.7f;
		float DepthTolerance = 0.8f;
		float FacingReflectionsFading = 0.1f;
		int MaxSteps = 70;
		uint32_t NumDepthMips;
		float RoughnessDepthTolerance = 1.0f;
		bool HalfRes = true;
		char Padding[3] { 0, 0, 0 };
		bool EnableConeTracing = true;
		char Padding1[3] { 0, 0, 0 };
		float LuminanceFactor = 1.0f;
	};

	struct SceneRendererCamera {
		ForgottenEngine::Camera Camera;
		glm::mat4 ViewMatrix;
		float Near, Far; // Non-reversed
		float FOV;
	};

	struct BloomSettings {
		bool Enabled = true;
		float Threshold = 1.0f;
		float Knee = 0.1f;
		float UpsampleScale = 1.0f;
		float Intensity = 1.0f;
		float DirtIntensity = 1.0f;
	};

	// struct SceneRendererSpecification {
	//	Tiering::Renderer::RendererTieringSettings Tiering;
	// };

	class SceneRenderer : public ReferenceCounted {
	public:
		struct Statistics {
			uint32_t DrawCalls = 0;
			uint32_t Meshes = 0;
			uint32_t Instances = 0;
			uint32_t SavedDraws = 0;

			float TotalGPUTime = 0.0f;
		};

	public:
		SceneRenderer(Reference<Scene> scene /*, SceneRendererSpecification specification = SceneRendererSpecification() */);
		virtual ~SceneRenderer();

		void Init();
		void InitMaterials();

		void Shutdown();

		// Should only be called at initialization.
		void InitOptions();

		void SetScene(Reference<Scene> scene);

		void SetViewportSize(uint32_t width, uint32_t height);

		void UpdateHBAOData();
		void UpdateGTAOData();

		void BeginScene(const SceneRendererCamera& camera);
		void EndScene();

		static void InsertGPUPerfMarker(Reference<RenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {});
		static void BeginGPUPerfMarker(Reference<RenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor = {});
		static void EndGPUPerfMarker(Reference<RenderCommandBuffer> renderCommandBuffer);

		void SubmitMesh(Reference<Mesh> mesh, uint32_t submeshIndex, Reference<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Reference<Material> overrideMaterial = nullptr);
		void SubmitStaticMesh(Reference<StaticMesh> staticMesh, Reference<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Reference<Material> overrideMaterial = nullptr);

		void SubmitSelectedMesh(Reference<Mesh> mesh, uint32_t submeshIndex, Reference<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Reference<Material> overrideMaterial = nullptr);
		void SubmitSelectedStaticMesh(Reference<StaticMesh> staticMesh, Reference<MaterialTable> materialTable, const glm::mat4& transform = glm::mat4(1.0f), Reference<Material> overrideMaterial = nullptr);

		void SubmitPhysicsDebugMesh(Reference<Mesh> mesh, uint32_t submeshIndex, const glm::mat4& transform = glm::mat4(1.0f));
		void SubmitPhysicsStaticDebugMesh(Reference<StaticMesh> mesh, const glm::mat4& transform = glm::mat4(1.0f), const bool isPrimitiveCollider = true);

		Reference<RenderPass> GetFinalRenderPass();
		Reference<RenderPass> GetCompositeRenderPass() { return m_CompositePipeline->get_specification().RenderPass; }
		Reference<RenderPass> GetExternalCompositeRenderPass() { return m_ExternalCompositeRenderPass; }
		Reference<Image2D> GetFinalPassImage();

		Reference<Renderer2D> GetRenderer2D() { return m_Renderer2D; }

		SceneRendererOptions& GetOptions();

		void SetShadowSettings(float nearPlane, float farPlane, float lambda)
		{
			CascadeNearPlaneOffset = nearPlane;
			CascadeFarPlaneOffset = farPlane;
			CascadeSplitLambda = lambda;
		}

		BloomSettings& GetBloomSettings() { return m_BloomSettings; }

		void SetLineWidth(float width);

		void OnImGuiRender();

		static void WaitForThreads();

		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		float GetOpacity() const { return m_Opacity; }
		void SetOpacity(float opacity) { m_Opacity = opacity; }

		const Statistics& GetStatistics() const { return m_Statistics; }

	private:
		void FlushDrawList();

		void PreRender();

		void ClearPass();
		void ClearPass(Reference<RenderPass> renderPass, bool explicitClear = false);
		void DeinterleavingPass();
		void HBAOCompute();
		void GTAOCompute();

		void AOComposite();

		void GTAODenoiseCompute();

		void ReinterleavingPass();
		void HBAOBlurPass();
		void ShadowMapPass();
		void PreDepthPass();
		void HZBCompute();
		void PreIntegration();
		void LightCullingPass();
		void GeometryPass();
		void PreConvolutionCompute();
		void JumpFloodPass();

		// Post-processing
		void BloomCompute();
		void SSRCompute();
		void SSRCompositePass();
		void CompositePass();

		struct CascadeData {
			glm::mat4 ViewProj;
			glm::mat4 View;
			float SplitDepth;
		};
		void CalculateCascades(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const;

		void UpdateStatistics();

	private:
		Reference<Scene> m_Scene;
		// SceneRendererSpecification m_Specification;
		Reference<RenderCommandBuffer> m_CommandBuffer;

		Reference<Renderer2D> m_Renderer2D;

		struct SceneInfo {
			SceneRendererCamera SceneCamera;

			// Resources
			Reference<SceneEnvironment> SceneEnvironment;
			float SkyboxLod = 0.0f;
			float SceneEnvironmentIntensity;
			LightEnvironment SceneLightEnvironment;
			DirLight ActiveLight;
		} m_SceneData;

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

		glm::uvec3 m_LightCullingWorkGroups;

		Reference<UniformBufferSet> m_UniformBufferSet;
		Reference<StorageBufferSet> m_StorageBufferSet;

		Reference<Shader> ShadowMapShader, ShadowMapAnimShader;
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

		bool EnableBloom = false;
		float BloomThreshold = 1.5f;

		glm::vec2 FocusPoint = { 0.5f, 0.5f };

		Reference<Material> m_CompositeMaterial;
		Reference<Material> m_LightCullingMaterial;

		Reference<Pipeline> m_GeometryPipeline;
		Reference<Pipeline> m_TransparentGeometryPipeline;
		Reference<Pipeline> m_SelectedGeometryPipeline;
		Reference<Pipeline> m_GeometryWireframePipeline;
		Reference<Pipeline> m_GeometryWireframeOnTopPipeline;
		Reference<Pipeline> m_PreDepthPipeline, m_PreDepthTransparentPipeline;
		Reference<Pipeline> m_CompositePipeline;
		Reference<Pipeline> m_ShadowPassPipelines[4];
		Reference<Material> m_ShadowPassMaterial;
		Reference<Material> m_PreDepthMaterial;
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

		std::vector<TransformBuffer> m_SubmeshTransformBuffers;

		Reference<Material> m_SelectedGeometryMaterial;

		std::vector<Reference<Framebuffer>> m_TempFramebuffers;

		Reference<RenderPass> m_ExternalCompositeRenderPass;

		struct DrawCommand {
			Reference<Mesh> Mesh;
			uint32_t SubmeshIndex;
			Reference<MaterialTable> MaterialTable;
			Reference<Material> OverrideMaterial;

			uint32_t InstanceCount = 0;
			uint32_t InstanceOffset = 0;
		};

		struct StaticDrawCommand {
			Reference<StaticMesh> StaticMesh;
			uint32_t SubmeshIndex;
			Reference<MaterialTable> MaterialTable;
			Reference<Material> OverrideMaterial;

			uint32_t InstanceCount = 0;
			uint32_t InstanceOffset = 0;
		};

		struct MeshKey {
			AssetHandle MeshHandle;
			AssetHandle MaterialHandle;
			uint32_t SubmeshIndex;
			bool IsSelected;

			MeshKey(AssetHandle meshHandle, AssetHandle materialHandle, uint32_t submeshIndex, bool isSelected)
				: MeshHandle(meshHandle)
				, MaterialHandle(materialHandle)
				, SubmeshIndex(submeshIndex)
				, IsSelected(isSelected)
			{
			}

			bool operator<(const MeshKey& other) const
			{
				if (MeshHandle < other.MeshHandle)
					return true;

				if (MeshHandle > other.MeshHandle)
					return false;

				if (SubmeshIndex < other.SubmeshIndex)
					return true;

				if (SubmeshIndex > other.SubmeshIndex)
					return false;

				if (MaterialHandle < other.MaterialHandle)
					return true;

				if (MaterialHandle > other.MaterialHandle)
					return false;

				return IsSelected < other.IsSelected;
			}
		};

		struct TransformMapData {
			std::vector<TransformVertexData> Transforms;
			uint32_t TransformOffset = 0;
		};

		std::map<MeshKey, TransformMapData> m_MeshTransformMap;

		// std::vector<DrawCommand> m_DrawList;
		std::map<MeshKey, DrawCommand> m_DrawList;
		std::map<MeshKey, DrawCommand> m_TransparentDrawList;
		std::map<MeshKey, DrawCommand> m_SelectedMeshDrawList;
		std::map<MeshKey, DrawCommand> m_ShadowPassDrawList;

		std::map<MeshKey, StaticDrawCommand> m_StaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_TransparentStaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_SelectedStaticMeshDrawList;
		std::map<MeshKey, StaticDrawCommand> m_StaticMeshShadowPassDrawList;

		// Debug
		std::map<MeshKey, StaticDrawCommand> m_StaticColliderDrawList;
		std::map<MeshKey, DrawCommand> m_ColliderDrawList;

		// Grid
		Reference<Pipeline> m_GridPipeline;
		Reference<Shader> m_GridShader;
		Reference<Material> m_GridMaterial;
		Reference<Material> m_OutlineMaterial, OutlineAnimMaterial;
		Reference<Material> m_SimpleColliderMaterial, m_ComplexColliderMaterial;
		Reference<Material> m_WireframeMaterial;

		SceneRendererOptions m_Options;
		SSROptionsUB m_SSROptions;

		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
		float m_InvViewportWidth = 0.f, m_InvViewportHeight = 0.f;
		bool m_NeedsResize = false;
		bool m_Active = false;
		bool m_ResourcesCreated = false;

		float m_LineWidth = 2.0f;

		BloomSettings m_BloomSettings;
		Reference<Texture2D> m_BloomDirtTexture;

		float m_Opacity = 1.0f;

		struct GPUTimeQueries {
			uint32_t ShadowMapPassQuery = UINT32_MAX;
			uint32_t DepthPrePassQuery = UINT32_MAX;
			uint32_t HierarchicalDepthQuery = UINT32_MAX;
			uint32_t PreIntegrationQuery = UINT32_MAX;
			uint32_t LightCullingPassQuery = UINT32_MAX;
			uint32_t GeometryPassQuery = UINT32_MAX;
			uint32_t PreConvolutionQuery = UINT32_MAX;
			uint32_t HBAOPassQuery = UINT32_MAX;
			uint32_t GTAOPassQuery = UINT32_MAX;
			uint32_t GTAODenoisePassQuery = UINT32_MAX;
			uint32_t AOCompositePassQuery = UINT32_MAX;
			uint32_t SSRQuery = UINT32_MAX;
			uint32_t SSRCompositeQuery = UINT32_MAX;
			uint32_t BloomComputePassQuery = UINT32_MAX;
			uint32_t JumpFloodPassQuery = UINT32_MAX;
			uint32_t CompositePassQuery = UINT32_MAX;
		} m_GPUTimeQueries;

		Statistics m_Statistics;
	};

} // namespace ForgottenEngine
