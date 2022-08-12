#include "fg_pch.hpp"

#include "render/SceneRenderer.hpp"

#include "render/Renderer.hpp"
#include "render/Renderer2D.hpp"
#include "render/SceneEnvironment.hpp"
#include "render/UniformBuffer.hpp"
#include "utilities/FileSystem.hpp"
#include "vulkan/VulkanComputePipeline.hpp"
#include "vulkan/VulkanMaterial.hpp"
#include "vulkan/VulkanRenderer.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

namespace ForgottenEngine {

	static std::vector<std::thread> s_ThreadPool;

	SceneRenderer::SceneRenderer(Reference<Scene> scene)
		: m_Scene(scene)
	{
		Init();
	}

	SceneRenderer::~SceneRenderer()
	{
		Shutdown();
	}

	void SceneRenderer::Init()
	{
		/*
		// Tiering
		{
			using namespace Tiering::Renderer;

			const auto& tiering = m_Specification.Tiering;

			RendererDataUB.SoftShadows = tiering.ShadowQuality == ShadowQualitySetting::High;

			m_Options.EnableGTAO = false;
			m_Options.EnableHBAO = false;

			if (tiering.EnableAO) {
				switch (tiering.AOType) {
				case Tiering::Renderer::AmbientOcclusionTypeSetting::HBAO:
					m_Options.EnableHBAO = true;
					break;
				case Tiering::Renderer::AmbientOcclusionTypeSetting::GTAO:
					m_Options.EnableGTAO = true;
					break;
				}
			}
		}*/

		m_CommandBuffer = RenderCommandBuffer::create(0);

		uint32_t framesInFlight = Renderer::get_config().frames_in_flight;
		m_UniformBufferSet = UniformBufferSet::create(framesInFlight);
		m_UniformBufferSet->create(sizeof(UBCamera), 0);
		m_UniformBufferSet->create(sizeof(UBShadow), 1);
		m_UniformBufferSet->create(sizeof(UBScene), 2);
		m_UniformBufferSet->create(sizeof(UBRendererData), 3);
		m_UniformBufferSet->create(sizeof(UBPointLights), 4);
		m_UniformBufferSet->create(sizeof(UBScreenData), 17);
		m_UniformBufferSet->create(sizeof(UBHBAOData), 18);

		m_Renderer2D = Reference<Renderer2D>::create();

		m_CompositeShader = Renderer::get_shader_library()->get("SceneComposite");
		m_CompositeMaterial = Material::create(m_CompositeShader);

		// Light culling compute pipeline
		{
			m_StorageBufferSet = StorageBufferSet::create(framesInFlight);
			m_StorageBufferSet->create(1, 14); // Can't allocate 0 bytes.. Resized later
			m_StorageBufferSet->create(1, 23);

			m_LightCullingMaterial = Material::create(Renderer::get_shader_library()->get("LightCulling"), "LightCulling");
			Reference<Shader> lightCullingShader = Renderer::get_shader_library()->get("LightCulling");
			m_LightCullingPipeline = ComputePipeline::create(lightCullingShader);
		}

		// Directional Shadow pass
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::DEPTH32F;
			spec.Usage = ImageUsage::Attachment;

			uint32_t shadowMapResolution = 4096;
			auto shadowMapResolution = 1024;
			spec.Width = shadowMapResolution;
			spec.Height = shadowMapResolution;
			spec.Layers = 4; // 4 cascades
			spec.DebugName = "ShadowCascades";
			Reference<Image2D> cascadedDepthImage = Image2D::create(spec);
			cascadedDepthImage->invalidate();
			cascadedDepthImage->create_per_layer_image_views();

			FramebufferSpecification shadowMapFramebufferSpec;
			shadowMapFramebufferSpec.Width = shadowMapResolution;
			shadowMapFramebufferSpec.Height = shadowMapResolution;
			shadowMapFramebufferSpec.Attachments = { ImageFormat::DEPTH32F };
			shadowMapFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			shadowMapFramebufferSpec.DepthClearValue = 1.0f;
			shadowMapFramebufferSpec.NoResize = true;
			shadowMapFramebufferSpec.ExistingImage = cascadedDepthImage;
			shadowMapFramebufferSpec.DebugName = "Dir Shadow Map";

			// 4 cascades
			auto shadowPassShader = Renderer::get_shader_library()->get("DirShadowMap");
			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "DirShadowPass";
			pipelineSpec.Shader = shadowPassShader;
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;

			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float3, "a_Normal" },
				{ ShaderDataType::Float3, "a_Tangent" },
				{ ShaderDataType::Float3, "a_Binormal" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.InstanceLayout = {
				{ ShaderDataType::Float4, "a_MRow0" },
				{ ShaderDataType::Float4, "a_MRow1" },
				{ ShaderDataType::Float4, "a_MRow2" },
			};

			// 4 cascades
			for (int i = 0; i < 4; i++) {
				shadowMapFramebufferSpec.ExistingImageLayers.clear();
				shadowMapFramebufferSpec.ExistingImageLayers.emplace_back(i);

				RenderPassSpecification shadowMapRenderPassSpec;
				shadowMapRenderPassSpec.TargetFramebuffer = Framebuffer::create(shadowMapFramebufferSpec);
				shadowMapRenderPassSpec.DebugName = "DirShadowMap";
				pipelineSpec.RenderPass = RenderPass::create(shadowMapRenderPassSpec);
				m_ShadowPassPipelines[i] = Pipeline::create(pipelineSpec);
			}

			m_ShadowPassMaterial = Material::create(shadowPassShader, "DirShadowPass");
		}

		// PreDepth
		{
			FramebufferSpecification preDepthFramebufferSpec;
			// Linear depth, reversed device depth
			preDepthFramebufferSpec.Attachments = { /*ImageFormat::RED32F, */ ImageFormat::DEPTH32FSTENCIL8UINT };
			preDepthFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			preDepthFramebufferSpec.DepthClearValue = 0.0f;
			preDepthFramebufferSpec.DebugName = "PreDepth-Opaque";

			RenderPassSpecification preDepthRenderPassSpec;
			preDepthRenderPassSpec.TargetFramebuffer = Framebuffer::create(preDepthFramebufferSpec);
			preDepthRenderPassSpec.DebugName = preDepthFramebufferSpec.DebugName;

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = preDepthFramebufferSpec.DebugName;
			Reference<Shader> shader = Renderer::get_shader_library()->get("PreDepth");
			pipelineSpec.Shader = shader;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float3, "a_Normal" },
				{ ShaderDataType::Float3, "a_Tangent" },
				{ ShaderDataType::Float3, "a_Binormal" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.InstanceLayout = {
				{ ShaderDataType::Float4, "a_MRow0" },
				{ ShaderDataType::Float4, "a_MRow1" },
				{ ShaderDataType::Float4, "a_MRow2" },
			};
			pipelineSpec.RenderPass = RenderPass::create(preDepthRenderPassSpec);
			m_PreDepthPipeline = Pipeline::create(pipelineSpec);
			m_PreDepthMaterial = Material::create(shader, "PreDepth");

			pipelineSpec.DebugName = "PreDepth-Transparent";
			preDepthFramebufferSpec.DebugName = pipelineSpec.DebugName;
			preDepthRenderPassSpec.TargetFramebuffer = Framebuffer::create(preDepthFramebufferSpec);
			preDepthRenderPassSpec.DebugName = pipelineSpec.DebugName;
			pipelineSpec.RenderPass = RenderPass::create(preDepthRenderPassSpec);
			m_PreDepthTransparentPipeline = Pipeline::create(pipelineSpec);
		}

		// Hierarchical Z buffer
		{
			TextureProperties properties;
			properties.SamplerWrap = TextureWrap::Clamp;
			properties.SamplerFilter = TextureFilter::Nearest;
			properties.DebugName = "HierarchicalZ";

			m_HierarchicalDepthTexture = Texture2D::create(ImageFormat::RED32F, 1, 1, nullptr, properties);

			Reference<Shader> shader = Renderer::get_shader_library()->get("HZB");
			m_HierarchicalDepthPipeline = ComputePipeline::create(shader);
			m_HierarchicalDepthMaterial = Material::create(shader, "HZB");
		}

		// Pre-Integration
		{
			TextureProperties properties;
			properties.DebugName = "Pre-Integration";

			m_VisibilityTexture = Texture2D::create(ImageFormat::RED8UN, 1, 1, nullptr, properties);

			Reference<Shader> shader = Renderer::get_shader_library()->get("Pre-Integration");
			m_PreIntegrationPipeline = ComputePipeline::create(shader);
			m_PreIntegrationMaterial = Material::create(shader, "Pre-Integration");
		}

		// Geometry
		{
			FramebufferSpecification geoFramebufferSpec;
			geoFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::RGBA16F, ImageFormat::RGBA, ImageFormat::DEPTH32FSTENCIL8UINT };
			geoFramebufferSpec.ExistingImages[3] = m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_depth_image();

			// Don't blend with luminance in the alpha channel.
			geoFramebufferSpec.Attachments.Attachments[1].Blend = false;
			geoFramebufferSpec.Samples = 1;
			geoFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			geoFramebufferSpec.DebugName = "Geometry";
			geoFramebufferSpec.ClearDepthOnLoad = false;

			Reference<Framebuffer> framebuffer = Framebuffer::create(geoFramebufferSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.LineWidth = m_LineWidth;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float3, "a_Normal" },
				{ ShaderDataType::Float3, "a_Tangent" },
				{ ShaderDataType::Float3, "a_Binormal" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.InstanceLayout = {
				{ ShaderDataType::Float4, "a_MRow0" },
				{ ShaderDataType::Float4, "a_MRow1" },
				{ ShaderDataType::Float4, "a_MRow2" },
			};
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("HazelPBR_Static");
			pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
			pipelineSpecification.DepthWrite = false;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.TargetFramebuffer = framebuffer;
			renderPassSpec.DebugName = "Geometry";
			pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
			pipelineSpecification.DebugName = "PBR-Static";
			m_GeometryPipeline = Pipeline::create(pipelineSpecification);

			//
			// Transparent Geometry
			//
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("HazelPBR_Transparent");
			pipelineSpecification.DebugName = "PBR-Transparent";
			pipelineSpecification.DepthOperator = DepthCompareOperator::GreaterOrEqual;
			m_TransparentGeometryPipeline = Pipeline::create(pipelineSpecification);
		}

		// Selected Geometry isolation (for outline pass)
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "SelectedGeometry";
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float3, "a_Normal" },
				{ ShaderDataType::Float3, "a_Tangent" },
				{ ShaderDataType::Float3, "a_Binormal" },
				{ ShaderDataType::Float2, "a_TexCoord" },
			};
			pipelineSpecification.InstanceLayout = {
				{ ShaderDataType::Float4, "a_MRow0" },
				{ ShaderDataType::Float4, "a_MRow1" },
				{ ShaderDataType::Float4, "a_MRow2" },
			};

			FramebufferSpecification framebufferSpec;
			framebufferSpec.DebugName = pipelineSpecification.DebugName;
			framebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			framebufferSpec.Samples = 1;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			framebufferSpec.DepthClearValue = 1.0f;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = pipelineSpecification.DebugName;
			renderPassSpec.TargetFramebuffer = Framebuffer::create(framebufferSpec);
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("SelectedGeometry");
			pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
			pipelineSpecification.DepthOperator = DepthCompareOperator::LessOrEqual;
			m_SelectedGeometryPipeline = Pipeline::create(pipelineSpecification);

			m_SelectedGeometryMaterial = Material::create(pipelineSpecification.Shader);
		}

		// Pre-convolution Compute
		{
			auto shader = Renderer::get_shader_library()->get("Pre-Convolution");
			m_GaussianBlurPipeline = ComputePipeline::create(shader);
			TextureProperties props;
			props.SamplerWrap = TextureWrap::Clamp;
			props.DebugName = "Pre-Convoluted";
			m_PreConvolutedTexture = Texture2D::create(ImageFormat::RGBA32F, 1, 1, nullptr, props);

			m_GaussianBlurMaterial = Material::create(shader);
		}

		// Bloom Compute
		{
			auto shader = Renderer::get_shader_library()->get("Bloom");
			m_BloomComputePipeline = ComputePipeline::create(shader);
			TextureProperties props;
			props.SamplerWrap = TextureWrap::Clamp;
			props.Storage = true;
			props.GenerateMips = true;
			props.DebugName = "BloomCompute-0";
			m_BloomComputeTextures[0] = Texture2D::create(ImageFormat::RGBA32F, 1, 1, nullptr, props);
			props.DebugName = "BloomCompute-1";
			m_BloomComputeTextures[1] = Texture2D::create(ImageFormat::RGBA32F, 1, 1, nullptr, props);
			props.DebugName = "BloomCompute-2";
			m_BloomComputeTextures[2] = Texture2D::create(ImageFormat::RGBA32F, 1, 1, nullptr, props);
			m_BloomComputeMaterial = Material::create(shader);

			m_BloomDirtTexture = Renderer::get_white_texture(); // BLACK SOON!
		}

		// Deinterleaving
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RED32F;
			imageSpec.Layers = 16;
			imageSpec.Usage = ImageUsage::Attachment;
			imageSpec.DebugName = "Deinterleaved";
			Reference<Image2D> image = Image2D::create(imageSpec);
			image->invalidate();
			image->create_per_layer_image_views();

			FramebufferSpecification deinterleavingFramebufferSpec;
			deinterleavingFramebufferSpec.Attachments.Attachments = { 8, FramebufferTextureSpecification { ImageFormat::RED32F } };
			deinterleavingFramebufferSpec.ClearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
			deinterleavingFramebufferSpec.DebugName = "Deinterleaving";
			deinterleavingFramebufferSpec.ExistingImage = image;

			Reference<Shader> shader = Renderer::get_shader_library()->get("Deinterleaving");
			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Deinterleaving";
			pipelineSpec.DepthWrite = false;
			pipelineSpec.DepthTest = false;

			pipelineSpec.Shader = shader;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};

			// 2 frame buffers, 2 render passes .. 8 attachments each
			for (int rp = 0; rp < 2; rp++) {
				deinterleavingFramebufferSpec.ExistingImageLayers.clear();
				for (int layer = 0; layer < 8; layer++)
					deinterleavingFramebufferSpec.ExistingImageLayers.emplace_back(rp * 8 + layer);

				Reference<Framebuffer> framebuffer = Framebuffer::create(deinterleavingFramebufferSpec);

				RenderPassSpecification deinterleavingRenderPassSpec;
				deinterleavingRenderPassSpec.TargetFramebuffer = framebuffer;
				deinterleavingRenderPassSpec.DebugName = "Deinterleaving";
				pipelineSpec.RenderPass = RenderPass::create(deinterleavingRenderPassSpec);

				m_DeinterleavingPipelines[rp] = Pipeline::create(pipelineSpec);
			}
			m_DeinterleavingMaterial = Material::create(shader, "Deinterleaving");
		}

		// HBAO
		{
			ImageSpecification imageSpec;
			imageSpec.Format = ImageFormat::RG16F;
			imageSpec.Usage = ImageUsage::Storage;
			imageSpec.Layers = 16;
			imageSpec.DebugName = "HBAO-Output";
			Reference<Image2D> image = Image2D::create(imageSpec);
			image->invalidate();
			image->create_per_layer_image_views();

			m_HBAOOutputImage = image;

			Reference<Shader> shader = Renderer::get_shader_library()->get("HBAO");

			m_HBAOPipeline = ComputePipeline::create(shader);
			m_HBAOMaterial = Material::create(shader, "HBAO");

			for (int i = 0; i < 16; i++)
				HBAODataUB.Float2Offsets[i] = glm::vec4((float)(i % 4) + 0.5f, (float)(i / 4) + 0.5f, 0.0f, 1.f);
			// std::memcpy(HBAODataUB.Jitters, Noise::HBAOJitter().data(), sizeof glm::vec4 * 16);
		}

		// GTAO
		{
			{
				ImageSpecification imageSpec;
				if (m_Options.GTAOBentNormals)
					imageSpec.Format = ImageFormat::RED32UI;
				else
					imageSpec.Format = ImageFormat::RED8UI;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.Layers = 1;
				imageSpec.DebugName = "GTAO";
				m_GTAOOutputImage = Image2D::create(imageSpec);
				m_GTAOOutputImage->invalidate();
			}

			// GTAO-Edges
			{
				ImageSpecification imageSpec;
				imageSpec.Format = ImageFormat::RED8UN;
				imageSpec.Usage = ImageUsage::Storage;
				imageSpec.DebugName = "GTAO-Edges";
				m_GTAOEdgesOutputImage = Image2D::create(imageSpec);
				m_GTAOEdgesOutputImage->invalidate();
			}

			Reference<Shader> shader = Renderer::get_shader_library()->get("GTAO");

			m_GTAOPipeline = ComputePipeline::create(shader);
			m_GTAOMaterial = Material::create(shader, "GTAO");
		}

		// GTAO Denoise
		{
			{ ImageSpecification imageSpec;
		if (m_Options.GTAOBentNormals)
			imageSpec.Format = ImageFormat::RED32UI;
		else
			imageSpec.Format = ImageFormat::RED8UI;
		imageSpec.Usage = ImageUsage::Storage;
		imageSpec.Layers = 1;
		imageSpec.DebugName = "GTAO-Denoise";
		m_GTAODenoiseImage = Image2D::create(imageSpec);
		m_GTAODenoiseImage->invalidate();

		Reference<Shader> shader = Renderer::get_shader_library()->get("GTAO-Denoise");
		m_GTAODenoiseMaterial[0] = Material::create(shader, "GTAO-Denoise-Ping");
		m_GTAODenoiseMaterial[1] = Material::create(shader, "GTAO-Denoise-Pong");

		m_GTAODenoisePipeline = ComputePipeline::create(shader);
	}

	// GTAO Composite
	{
		PipelineSpecification aoCompositePipelineSpec;
		aoCompositePipelineSpec.DebugName = "AO-Composite";
		RenderPassSpecification renderPassSpec;
		renderPassSpec.DebugName = "AO-Composite";
		FramebufferSpecification framebufferSpec;
		framebufferSpec.DebugName = "AO-Composite";
		framebufferSpec.Attachments = { ImageFormat::RGBA32F };
		framebufferSpec.ExistingImages[0] = m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
		framebufferSpec.Blend = true;
		framebufferSpec.ClearColorOnLoad = false;
		framebufferSpec.BlendMode = FramebufferBlendMode::Zero_SrcColor;

		renderPassSpec.TargetFramebuffer = Framebuffer::create(framebufferSpec);
		aoCompositePipelineSpec.RenderPass = RenderPass::create(renderPassSpec);
		m_AOCompositeRenderPass = aoCompositePipelineSpec.RenderPass;
		aoCompositePipelineSpec.DepthTest = false;
		aoCompositePipelineSpec.Layout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" },
		};
		aoCompositePipelineSpec.Shader = Renderer::get_shader_library()->get("AO-Composite");
		m_AOCompositePipeline = Pipeline::create(aoCompositePipelineSpec);
		m_AOCompositeMaterial = Material::create(aoCompositePipelineSpec.Shader, "GTAO-Composite");
	}
} // namespace ForgottenEngine

// Reinterleaving
{
	FramebufferSpecification reinterleavingFramebufferSpec;
	reinterleavingFramebufferSpec.Attachments = {
		ImageFormat::RG16F,
	};
	reinterleavingFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	reinterleavingFramebufferSpec.DebugName = "Reinterleaving";

	Reference<Framebuffer> framebuffer = Framebuffer::create(reinterleavingFramebufferSpec);
	Reference<Shader> shader = Renderer::get_shader_library()->get("Reinterleaving");
	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" },
	};
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.Shader = shader;
	pipelineSpecification.DepthTest = false;
	pipelineSpecification.DepthWrite = false;

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = framebuffer;
	renderPassSpec.DebugName = "Reinterleaving";
	pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
	pipelineSpecification.DebugName = "Reinterleaving";
	pipelineSpecification.DepthWrite = false;
	m_ReinterleavingPipeline = Pipeline::create(pipelineSpecification);

	m_ReinterleavingMaterial = Material::create(shader, "Reinterleaving");
}

// HBAO Blur
{
	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" },
	};
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.DepthTest = false;
	pipelineSpecification.DepthWrite = false;

	auto shader = Renderer::get_shader_library()->get("HBAOBlur");

	FramebufferSpecification hbaoBlurFramebufferSpec;
	hbaoBlurFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	hbaoBlurFramebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::RG16F);

	RenderPassSpecification renderPassSpec;
	renderPassSpec.DebugName = hbaoBlurFramebufferSpec.DebugName;

	pipelineSpecification.Shader = shader;
	pipelineSpecification.DebugName = "HBAOBlur";
	// First blur pass
	{
		pipelineSpecification.DebugName = "HBAOBlur1";
		hbaoBlurFramebufferSpec.DebugName = "HBAOBlur1";
		renderPassSpec.TargetFramebuffer = Framebuffer::create(hbaoBlurFramebufferSpec);
		pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
		m_HBAOBlurPipelines[0] = Pipeline::create(pipelineSpecification);
	}
	// Second blur pass
	{
		pipelineSpecification.DebugName = "HBAOBlur2";
		hbaoBlurFramebufferSpec.DebugName = "HBAOBlur2";
		renderPassSpec.TargetFramebuffer = Framebuffer::create(hbaoBlurFramebufferSpec);
		pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
		m_HBAOBlurPipelines[1] = Pipeline::create(pipelineSpecification);
	}
	m_HBAOBlurMaterials[0] = Material::create(shader, "HBAOBlur");
	m_HBAOBlurMaterials[1] = Material::create(shader, "HBAOBlur2");
}

// SSR
{
	ImageSpecification imageSpec;
	imageSpec.Format = ImageFormat::RGBA16F;
	imageSpec.Usage = ImageUsage::Storage;
	imageSpec.DebugName = "SSR";
	Reference<Image2D> image = Image2D::create(imageSpec);
	image->invalidate();

	m_SSRImage = image;

	Reference<Shader> shader = Renderer::get_shader_library()->get("SSR");

	m_SSRPipeline = ComputePipeline::create(shader);
	m_SSRMaterial = Material::create(shader, "SSR");
}

// SSR Composite
{
	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" },
	};
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.DepthTest = false;
	pipelineSpecification.DepthWrite = false;
	pipelineSpecification.DebugName = "SSR-Composite";
	auto shader = Renderer::get_shader_library()->get("SSR-Composite");
	pipelineSpecification.Shader = shader;

	FramebufferSpecification framebufferSpec;
	framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	framebufferSpec.Attachments.Attachments.emplace_back(ImageFormat::RGBA32F);
	framebufferSpec.ExistingImages[0] = m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
	framebufferSpec.DebugName = "SSR-Composite";
	framebufferSpec.BlendMode = FramebufferBlendMode::SrcAlphaOneMinusSrcAlpha;
	framebufferSpec.ClearColorOnLoad = false;

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = Framebuffer::create(framebufferSpec);
	renderPassSpec.DebugName = framebufferSpec.DebugName;
	pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);

	m_SSRCompositePipeline = Pipeline::create(pipelineSpecification);
	m_SSRCompositeMaterial = Material::create(shader, "SSR-Composite");
}

// Composite
{
	FramebufferSpecification compFramebufferSpec;
	compFramebufferSpec.DebugName = "SceneComposite";
	compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	compFramebufferSpec.Attachments = { ImageFormat::RGBA, ImageFormat::Depth };

	Reference<Framebuffer> framebuffer = Framebuffer::create(compFramebufferSpec);

	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" }
	};
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.Shader = Renderer::get_shader_library()->get("SceneComposite");

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = framebuffer;
	renderPassSpec.DebugName = "SceneComposite";
	pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
	pipelineSpecification.DebugName = "SceneComposite";
	pipelineSpecification.DepthWrite = false;
	pipelineSpecification.DepthTest = false;
	m_CompositePipeline = Pipeline::create(pipelineSpecification);
}

// DOF
{
	FramebufferSpecification compFramebufferSpec;
	compFramebufferSpec.DebugName = "POST-DepthOfField";
	compFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	compFramebufferSpec.Attachments = { ImageFormat::RGBA, ImageFormat::Depth };

	Reference<Framebuffer> framebuffer = Framebuffer::create(compFramebufferSpec);

	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" }
	};
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.Shader = Renderer::get_shader_library()->get("DOF");

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = framebuffer;
	renderPassSpec.DebugName = compFramebufferSpec.DebugName;
	pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
	pipelineSpecification.DebugName = compFramebufferSpec.DebugName;
	pipelineSpecification.DepthWrite = false;
	m_DOFPipeline = Pipeline::create(pipelineSpecification);
	m_DOFMaterial = Material::create(pipelineSpecification.Shader, "POST-DepthOfField");
}

// External compositing
{
	FramebufferSpecification extCompFramebufferSpec;
	extCompFramebufferSpec.Attachments = { ImageFormat::RGBA, ImageFormat::DEPTH32FSTENCIL8UINT };
	extCompFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	extCompFramebufferSpec.ClearColorOnLoad = false;
	extCompFramebufferSpec.ClearDepthOnLoad = false;
	extCompFramebufferSpec.DebugName = "External-Composite";

	// Use the color buffer from the final compositing pass, but the depth buffer from
	// the actual 3D geometry pass, in case we want to composite elements behind meshes
	// in the scene
	extCompFramebufferSpec.ExistingImages[0] = m_CompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
	extCompFramebufferSpec.ExistingImages[1] = m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_depth_image();

	Reference<Framebuffer> framebuffer = Framebuffer::create(extCompFramebufferSpec);

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = framebuffer;
	renderPassSpec.DebugName = "External-Composite";
	m_ExternalCompositeRenderPass = RenderPass::create(renderPassSpec);

	PipelineSpecification pipelineSpecification;
	pipelineSpecification.LineWidth = m_LineWidth;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float3, "a_Normal" },
		{ ShaderDataType::Float3, "a_Tangent" },
		{ ShaderDataType::Float3, "a_Binormal" },
		{ ShaderDataType::Float2, "a_TexCoord" },
	};
	pipelineSpecification.InstanceLayout = {
		{ ShaderDataType::Float4, "a_MRow0" },
		{ ShaderDataType::Float4, "a_MRow1" },
		{ ShaderDataType::Float4, "a_MRow2" },
	};
	pipelineSpecification.BackfaceCulling = false;
	pipelineSpecification.Wireframe = true;
	pipelineSpecification.DepthTest = true;
	pipelineSpecification.LineWidth = 2.0f;
	pipelineSpecification.Shader = Renderer::get_shader_library()->get("Wireframe");
	pipelineSpecification.RenderPass = m_ExternalCompositeRenderPass;
	pipelineSpecification.DebugName = "Wireframe";
	m_GeometryWireframePipeline = Pipeline::create(pipelineSpecification);
	pipelineSpecification.DepthTest = false;
	m_GeometryWireframeOnTopPipeline = Pipeline::create(pipelineSpecification);
}

// Temporary framebuffers for re-use
{
	FramebufferSpecification framebufferSpec;
	framebufferSpec.Attachments = { ImageFormat::RGBA32F };
	framebufferSpec.Samples = 1;
	framebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
	framebufferSpec.BlendMode = FramebufferBlendMode::OneZero;
	framebufferSpec.DebugName = "Temporaries";

	for (uint32_t i = 0; i < 2; i++)
		m_TempFramebuffers.emplace_back(Framebuffer::create(framebufferSpec));
}

// Jump Flood (outline)
{
	PipelineSpecification pipelineSpecification;
	pipelineSpecification.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" }
	};
	pipelineSpecification.Shader = Renderer::get_shader_library()->get("JumpFlood_Init");
	m_JumpFloodInitMaterial = Material::create(pipelineSpecification.Shader, "JumpFlood-Init");

	RenderPassSpecification renderPassSpec;
	renderPassSpec.TargetFramebuffer = m_TempFramebuffers[0];
	renderPassSpec.DebugName = "JumpFlood-Init";
	pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
	pipelineSpecification.DebugName = "JumpFlood-Init";
	m_JumpFloodInitPipeline = Pipeline::create(pipelineSpecification);

	pipelineSpecification.Shader = Renderer::get_shader_library()->get("JumpFlood_Pass");
	m_JumpFloodPassMaterial[0] = Material::create(pipelineSpecification.Shader, "JumpFloodPass-0");
	m_JumpFloodPassMaterial[1] = Material::create(pipelineSpecification.Shader, "JumpFloodPass-1");

	const char* passName[2] = { "EvenPass", "OddPass" };
	for (uint32_t i = 0; i < 2; i++) {
		renderPassSpec.TargetFramebuffer = m_TempFramebuffers[(i + 1) % 2];
		renderPassSpec.DebugName = fmt::format("JumpFlood-{0}", passName[i]);

		pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
		pipelineSpecification.DebugName = renderPassSpec.DebugName;

		m_JumpFloodPassPipeline[i] = Pipeline::create(pipelineSpecification);
	}

	// Outline compositing
	{
		pipelineSpecification.RenderPass = m_CompositePipeline->get_specification().RenderPass;
		pipelineSpecification.Shader = Renderer::get_shader_library()->get("JumpFlood_Composite");
		pipelineSpecification.DebugName = "JumpFlood-Composite";
		pipelineSpecification.DepthTest = false;
		m_JumpFloodCompositePipeline = Pipeline::create(pipelineSpecification);

		m_JumpFloodCompositeMaterial = Material::create(pipelineSpecification.Shader, "JumpFlood-Composite");
	}
}

// Grid
{
	m_GridShader = Renderer::get_shader_library()->get("Grid");
	const float gridScale = 16.025f;
	const float gridSize = 0.025f;
	m_GridMaterial = Material::create(m_GridShader);
	m_GridMaterial->set("u_Settings.Scale", gridScale);
	m_GridMaterial->set("u_Settings.Size", gridSize);

	PipelineSpecification pipelineSpec;
	pipelineSpec.DebugName = "Grid";
	pipelineSpec.Shader = m_GridShader;
	pipelineSpec.BackfaceCulling = false;
	pipelineSpec.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" }
	};
	pipelineSpec.RenderPass = m_ExternalCompositeRenderPass;
	m_GridPipeline = Pipeline::create(pipelineSpec);
}

// Collider
m_WireframeMaterial = Material::create(Renderer::get_shader_library()->get("Wireframe"));
m_WireframeMaterial->set("u_MaterialUniforms.Color", glm::vec4 { 1.0f, 0.5f, 0.0f, 1.0f });
m_SimpleColliderMaterial = Material::create(Renderer::get_shader_library()->get("Wireframe"));
m_SimpleColliderMaterial->set("u_MaterialUniforms.Color", m_Options.SimplePhysicsCollidersColor);
m_ComplexColliderMaterial = Material::create(Renderer::get_shader_library()->get("Wireframe"));
m_ComplexColliderMaterial->set("u_MaterialUniforms.Color", m_Options.ComplexPhysicsCollidersColor);

// Skybox
{
	auto skyboxShader = Renderer::get_shader_library()->get("Skybox");

	PipelineSpecification pipelineSpec;
	pipelineSpec.DebugName = "Skybox";
	pipelineSpec.Shader = skyboxShader;
	pipelineSpec.DepthWrite = false;
	pipelineSpec.DepthTest = false;
	pipelineSpec.Layout = {
		{ ShaderDataType::Float3, "a_Position" },
		{ ShaderDataType::Float2, "a_TexCoord" }
	};
	pipelineSpec.RenderPass = m_GeometryPipeline->get_specification().RenderPass;
	m_SkyboxPipeline = Pipeline::create(pipelineSpec);

	m_SkyboxMaterial = Material::create(skyboxShader);
	m_SkyboxMaterial->set_flag(MaterialFlag::DepthTest, false);
}

// TODO(Yan): resizeable/flushable
const size_t TransformBufferCount = 10 * 1024; // 10240 transforms
m_SubmeshTransformBuffers.resize(framesInFlight);
for (uint32_t i = 0; i < framesInFlight; i++) {
	m_SubmeshTransformBuffers[i].Buffer = VertexBuffer::create(sizeof(TransformVertexData) * TransformBufferCount);
	m_SubmeshTransformBuffers[i].Data = hnew TransformVertexData[TransformBufferCount];
}

Renderer::submit([instance = Reference(this)]() mutable { instance->m_ResourcesCreated = true; });

InitMaterials();
InitOptions();
}

void SceneRenderer::InitMaterials()
{
	Reference<SceneRenderer> instance = this;
	Renderer::submit([instance]() mutable {
		instance->m_HBAOBlurMaterials[0]->set("u_InputTex", instance->m_ReinterleavingPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0));
		instance->m_HBAOBlurMaterials[1]->set("u_InputTex", instance->m_HBAOBlurPipelines[0]->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0));

		instance->m_GTAODenoiseMaterial[0]->set("u_Edges", instance->m_GTAOEdgesOutputImage);
		instance->m_GTAODenoiseMaterial[0]->set("u_AOTerm", instance->m_GTAOOutputImage);
		instance->m_GTAODenoiseMaterial[0]->set("o_AOTerm", instance->m_GTAODenoiseImage);

		instance->m_GTAODenoiseMaterial[1]->set("u_Edges", instance->m_GTAOEdgesOutputImage);
		instance->m_GTAODenoiseMaterial[1]->set("u_AOTerm", instance->m_GTAODenoiseImage);
		instance->m_GTAODenoiseMaterial[1]->set("o_AOTerm", instance->m_GTAOOutputImage);
	});
}

void SceneRenderer::Shutdown()
{
	for (auto& transformBuffer : m_SubmeshTransformBuffers)
		hdelete[] transformBuffer.Data;
}

void SceneRenderer::InitOptions()
{
	// TODO(Karim): Deserialization?
	if (m_Options.EnableGTAO)
		*(int*)&m_Options.ReflectionOcclusionMethod |= (int)ShaderDef::AOMethod::GTAO;
	if (m_Options.EnableHBAO)
		*(int*)&m_Options.ReflectionOcclusionMethod |= (int)ShaderDef::AOMethod::HBAO;

	// Special macros are strictly starting with "__HZ_"
	Renderer::set_global_macro_in_shaders("__HZ_REFLECTION_OCCLUSION_METHOD", fmt::format("{}", (int)m_Options.ReflectionOcclusionMethod));
	Renderer::set_global_macro_in_shaders("__HZ_AO_METHOD", fmt::format("{}", ShaderDef::GetAOMethod(m_Options.EnableGTAO, m_Options.EnableHBAO)));
	Renderer::set_global_macro_in_shaders("__HZ_GTAO_COMPUTE_BENT_NORMALS", fmt::format("{}", (int)m_Options.GTAOBentNormals));
}

void SceneRenderer::InsertGPUPerfMarker(Reference<RenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor)
{
	Renderer::submit([=] {
		// Renderer::RT_InsertGPUPerfMarker(renderCommandBuffer, label, markerColor);
	});
}

void SceneRenderer::BeginGPUPerfMarker(Reference<RenderCommandBuffer> renderCommandBuffer, const std::string& label, const glm::vec4& markerColor)
{
	Renderer::submit([=] {
		// Renderer::RT_BeginGPUPerfMarker(renderCommandBuffer, label, markerColor);
	});
}

void SceneRenderer::EndGPUPerfMarker(Reference<RenderCommandBuffer> renderCommandBuffer)
{
	Renderer::submit([=] {
		// Renderer::RT_EndGPUPerfMarker(renderCommandBuffer);
	});
}

void SceneRenderer::SetScene(Reference<Scene> scene)
{
	CORE_ASSERT(!m_Active, "Can't change scenes while rendering");
	m_Scene = scene;
}

void SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
{
	width *= 1; // Tiering settings...
	height *= 1; // Tiering settings...

	if (m_ViewportWidth != width || m_ViewportHeight != height) {
		m_ViewportWidth = width;
		m_ViewportHeight = height;
		m_InvViewportWidth = 1.f / (float)width;
		m_InvViewportHeight = 1.f / (float)height;
		m_NeedsResize = true;
	}
}

void SceneRenderer::UpdateHBAOData()
{
	const auto& opts = m_Options;
	UBHBAOData& hbaoData = HBAODataUB;

	// radius
	const float meters2viewSpace = 1.0f;
	const float R = opts.HBAORadius * meters2viewSpace;
	const float R2 = R * R;
	hbaoData.NegInvR2 = -1.0f / R2;
	hbaoData.InvQuarterResolution = 1.f / glm::vec2 { (float)m_ViewportWidth / 4, (float)m_ViewportHeight / 4 };
	hbaoData.RadiusToScreen = R * 0.5f * (float)m_ViewportHeight / (tanf(m_SceneData.SceneCamera.FOV * 0.5f) * 2.0f);

	const float* P = glm::value_ptr(m_SceneData.SceneCamera.Camera.get());
	const glm::vec4 projInfoPerspective = {
		2.0f / (P[4 * 0 + 0]), // (x) * (R - L)/N
		2.0f / (P[4 * 1 + 1]), // (y) * (T - B)/N
		-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0], // L/N
		-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1], // B/N
	};
	hbaoData.PerspectiveInfo = projInfoPerspective;
	hbaoData.IsOrtho = false; // TODO: change depending on camera
	hbaoData.PowExponent = glm::max(opts.HBAOIntensity, 0.f);
	hbaoData.NDotVBias = glm::min(std::max(0.f, opts.HBAOBias), 1.f);
	hbaoData.AOMultiplier = 1.f / (1.f - hbaoData.NDotVBias);
	hbaoData.ShadowTolerance = m_Options.AOShadowTolerance;
}

// Some other settings are directly set in gui
void SceneRenderer::UpdateGTAOData()
{
	CBGTAOData& gtaoData = GTAODataCB;
	gtaoData.NDCToViewMul_x_PixelSize = { CameraDataUB.NDCToViewMul * (gtaoData.HalfRes ? ScreenDataUB.InvHalfResolution : ScreenDataUB.InvFullResolution) };
	gtaoData.HZBUVFactor = m_SSROptions.HZBUvFactor;
	gtaoData.ShadowTolerance = m_Options.AOShadowTolerance;
}

void SceneRenderer::BeginScene(const SceneRendererCamera& camera)
{

	CORE_ASSERT(m_Scene);
	CORE_ASSERT(!m_Active);
	m_Active = true;

	const bool updatedAnyShaders = Renderer::update_dirty_shaders();
	if (updatedAnyShaders)
		InitMaterials();

	if (!m_ResourcesCreated)
		return;

	m_GTAOFinalImage = m_Options.GTAODenoisePasses && m_Options.GTAODenoisePasses % 2 != 0 ? m_GTAODenoiseImage : m_GTAOOutputImage;

	m_SceneData.SceneCamera = camera;
	m_SceneData.SceneEnvironment = m_Scene->m_Environment;
	m_SceneData.SceneEnvironmentIntensity = m_Scene->m_EnvironmentIntensity;
	m_SceneData.ActiveLight = m_Scene->m_Light;
	m_SceneData.SceneLightEnvironment = m_Scene->m_LightEnvironment;
	m_SceneData.SkyboxLod = m_Scene->m_SkyboxLod;

	if (m_NeedsResize) {
		m_NeedsResize = false;

		const glm::uvec2 viewportSize = { m_ViewportWidth, m_ViewportHeight };

		ScreenDataUB.FullResolution = { m_ViewportWidth, m_ViewportHeight };
		ScreenDataUB.InvFullResolution = { m_InvViewportWidth, m_InvViewportHeight };
		ScreenDataUB.HalfResolution = glm::ivec2 { m_ViewportWidth, m_ViewportHeight } / 2;
		ScreenDataUB.InvHalfResolution = { m_InvViewportWidth * 2.0f, m_InvViewportHeight * 2.0f };

		// Both Pre-depth and geometry framebuffers need to be resized first.
		m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);
		m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);
		m_SelectedGeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);

		// Dependent on Geometry
		m_SSRCompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);

		m_VisibilityTexture->resize(m_ViewportWidth, m_ViewportHeight);
		m_ReinterleavingPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);
		m_HBAOBlurPipelines[0]->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);
		m_HBAOBlurPipelines[1]->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);
		m_AOCompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true); // Only resize after geometry framebuffer
		m_CompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight, true);

		// HZB
		{
			// HZB size must be power of 2's
			const glm::uvec2 numMips = glm::ceil(glm::log2(glm::vec2(viewportSize)));
			m_SSROptions.NumDepthMips = glm::max(numMips.x, numMips.y);

			const glm::uvec2 hzbSize = BIT(numMips);
			m_HierarchicalDepthTexture->resize(hzbSize.x, hzbSize.y);

			const glm::vec2 hzbUVFactor = { (glm::vec2)viewportSize / (glm::vec2)hzbSize };
			m_SSROptions.HZBUvFactor = hzbUVFactor;
		}

		// Light culling
		{
			constexpr uint32_t TILE_SIZE = 16u;
			glm::uvec2 size = viewportSize;
			size += TILE_SIZE - viewportSize % TILE_SIZE;
			m_LightCullingWorkGroups = { size / TILE_SIZE, 1 };
			RendererDataUB.TilesCountX = m_LightCullingWorkGroups.x;

			m_StorageBufferSet->resize(14, 0, m_LightCullingWorkGroups.x * m_LightCullingWorkGroups.y * 4 * 1024);
		}

		// GTAO
		{
			glm::uvec2 gtaoSize = GTAODataCB.HalfRes ? (viewportSize + 1u) / 2u : viewportSize;
			glm::uvec2 denoiseSize = gtaoSize;
			const ImageFormat gtaoImageFormat = m_Options.GTAOBentNormals ? ImageFormat::RED32UI : ImageFormat::RED8UI;
			m_GTAOOutputImage->get_specification().Format = gtaoImageFormat;
			m_GTAODenoiseImage->get_specification().Format = gtaoImageFormat;

			m_GTAOOutputImage->resize(gtaoSize);
			m_GTAODenoiseImage->resize(gtaoSize);
			m_GTAOEdgesOutputImage->resize(gtaoSize);

			constexpr uint32_t WORK_GROUP_SIZE = 16u;
			gtaoSize += WORK_GROUP_SIZE - gtaoSize % WORK_GROUP_SIZE;
			m_GTAOWorkGroups.x = gtaoSize.x / WORK_GROUP_SIZE;
			m_GTAOWorkGroups.y = gtaoSize.y / WORK_GROUP_SIZE;

			constexpr uint32_t DENOISE_WORK_GROUP_SIZE = 8u;
			denoiseSize += DENOISE_WORK_GROUP_SIZE - denoiseSize % DENOISE_WORK_GROUP_SIZE;
			m_GTAODenoiseWorkGroups.x = (denoiseSize.x + 2u * DENOISE_WORK_GROUP_SIZE - 1u) / (DENOISE_WORK_GROUP_SIZE * 2u); // 2 horizontal pixels at a time.
			m_GTAODenoiseWorkGroups.y = denoiseSize.y / DENOISE_WORK_GROUP_SIZE;
		}

		// Quarter-size hbao textures
		{
			glm::uvec2 quarterSize = (viewportSize + 3u) / 4u;
			m_DeinterleavingPipelines[0]->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(quarterSize.x, quarterSize.y);
			m_DeinterleavingPipelines[1]->get_specification().RenderPass->get_specification().TargetFramebuffer->resize(quarterSize.x, quarterSize.y);
			m_HBAOOutputImage->resize(quarterSize);
			m_HBAOOutputImage->create_per_layer_image_views();

			constexpr uint32_t WORK_GROUP_SIZE = 16u;
			quarterSize += WORK_GROUP_SIZE - quarterSize % WORK_GROUP_SIZE;
			m_HBAOWorkGroups.x = quarterSize.x / 16u;
			m_HBAOWorkGroups.y = quarterSize.y / 16u;
			m_HBAOWorkGroups.z = 16;
		}

		// SSR
		{
			constexpr uint32_t WORK_GROUP_SIZE = 8u;
			glm::uvec2 ssrSize = m_SSROptions.HalfRes ? (viewportSize + 1u) / 2u : viewportSize;
			m_SSRImage->resize(ssrSize);
			m_PreConvolutedTexture->resize(ssrSize.x, ssrSize.y);
			ssrSize += WORK_GROUP_SIZE - ssrSize % WORK_GROUP_SIZE;
			m_SSRWorkGroups.x = ssrSize.x / WORK_GROUP_SIZE;
			m_SSRWorkGroups.y = ssrSize.y / WORK_GROUP_SIZE;
		}

		// Bloom
		{
			glm::uvec2 bloomSize = (viewportSize + 1u) / 2u;
			bloomSize += m_BloomComputeWorkgroupSize - bloomSize % m_BloomComputeWorkgroupSize;
			m_BloomComputeTextures[0]->resize(bloomSize);
			m_BloomComputeTextures[1]->resize(bloomSize);
			m_BloomComputeTextures[2]->resize(bloomSize);
		}

		for (auto& tempFB : m_TempFramebuffers)
			tempFB->resize(m_ViewportWidth, m_ViewportHeight);

		if (m_ExternalCompositeRenderPass)
			m_ExternalCompositeRenderPass->get_specification().TargetFramebuffer->resize(m_ViewportWidth, m_ViewportHeight);
	}

	// Update uniform buffers
	UBCamera& cameraData = CameraDataUB;
	UBScene& sceneData = SceneDataUB;
	UBShadow& shadowData = ShadowData;
	UBRendererData& rendererData = RendererDataUB;
	UBPointLights& pointLightData = PointLightsUB;
	UBHBAOData& hbaoData = HBAODataUB;
	UBScreenData& screenData = ScreenDataUB;

	auto& sceneCamera = m_SceneData.SceneCamera;
	const auto viewProjection = sceneCamera.Camera.get_projection_matrix() * sceneCamera.ViewMatrix;
	const glm::mat4 viewInverse = glm::inverse(sceneCamera.ViewMatrix);
	const glm::mat4 projectionInverse = glm::inverse(sceneCamera.Camera.get_projection_matrix());
	const glm::vec3 cameraPosition = viewInverse[3];

	cameraData.ViewProjection = viewProjection;
	cameraData.Projection = sceneCamera.Camera.get_projection_matrix();
	cameraData.InverseProjection = projectionInverse;
	cameraData.View = sceneCamera.ViewMatrix;
	cameraData.InverseView = viewInverse;
	cameraData.InverseViewProjection = viewInverse * cameraData.InverseProjection;

	float depthLinearizeMul = (-cameraData.Projection[3][2]); // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
	float depthLinearizeAdd = (cameraData.Projection[2][2]); // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
	// correct the handedness issue.
	if (depthLinearizeMul * depthLinearizeAdd < 0)
		depthLinearizeAdd = -depthLinearizeAdd;
	cameraData.DepthUnpackConsts = { depthLinearizeMul, depthLinearizeAdd };
	const float* P = glm::value_ptr(m_SceneData.SceneCamera.Camera.get_projection_matrix());
	const glm::vec4 projInfoPerspective = {
		2.0f / (P[4 * 0 + 0]), // (x) * (R - L)/N
		2.0f / (P[4 * 1 + 1]), // (y) * (T - B)/N
		-(1.0f - P[4 * 2 + 0]) / P[4 * 0 + 0], // L/N
		-(1.0f + P[4 * 2 + 1]) / P[4 * 1 + 1], // B/N
	};
	float tanHalfFOVY = 1.0f / cameraData.Projection[1][1]; // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
	float tanHalfFOVX = 1.0f / cameraData.Projection[0][0]; // = tanHalfFOVY * drawContext.Camera.GetAspect( );
	cameraData.CameraTanHalfFOV = { tanHalfFOVX, tanHalfFOVY };
	cameraData.NDCToViewMul = { projInfoPerspective[0], projInfoPerspective[1] };
	cameraData.NDCToViewAdd = { projInfoPerspective[2], projInfoPerspective[3] };

	Reference<SceneRenderer> instance = this;
	Renderer::submit([instance, cameraData]() mutable {
		uint32_t bufferIndex = Renderer::get_current_frame_index();
		instance->m_UniformBufferSet->get(0, 0, bufferIndex)->rt_set_data(&cameraData, sizeof(cameraData));
	});

	const auto lightEnvironment = m_SceneData.SceneLightEnvironment;
	const std::vector<PointLight>& pointLightsVec = lightEnvironment.PointLights;
	pointLightData.Count = int(pointLightsVec.size());
	std::memcpy(pointLightData.PointLights, pointLightsVec.data(), lightEnvironment.GetPointLightsSize()); //(Karim) Do we really have to copy that?
	Renderer::submit([instance, &pointLightData]() mutable {
		const uint32_t bufferIndex = Renderer::get_current_frame_index();
		Reference<UniformBuffer> bufferSet = instance->m_UniformBufferSet->get(4, 0, bufferIndex);
		bufferSet->rt_set_data(&pointLightData, 16ull + sizeof PointLight * pointLightData.Count);
	});

	const auto& directionalLight = m_SceneData.SceneLightEnvironment.DirectionalLights[0];
	sceneData.Lights.Direction = directionalLight.Direction;
	sceneData.Lights.Radiance = directionalLight.Radiance;
	sceneData.Lights.Intensity = directionalLight.Intensity;
	sceneData.Lights.ShadowAmount = directionalLight.ShadowAmount;

	sceneData.CameraPosition = cameraPosition;
	sceneData.EnvironmentMapIntensity = m_SceneData.SceneEnvironmentIntensity;
	Renderer::submit([instance, sceneData]() mutable {
		uint32_t bufferIndex = Renderer::get_current_frame_index();
		instance->m_UniformBufferSet->get(2, 0, bufferIndex)->rt_set_data(&sceneData, sizeof(sceneData));
	});

	if (m_Options.EnableHBAO)
		UpdateHBAOData();
	if (m_Options.EnableGTAO)
		UpdateGTAOData();

	Renderer::submit([instance, hbaoData]() mutable {
		const uint32_t bufferIndex = Renderer::get_current_frame_index();
		instance->m_UniformBufferSet->get(18, 0, bufferIndex)->rt_set_data(&hbaoData, sizeof(hbaoData));
	});

	Renderer::submit([instance, screenData]() mutable {
		const uint32_t bufferIndex = Renderer::get_current_frame_index();
		instance->m_UniformBufferSet->get(17, 0, bufferIndex)->rt_set_data(&screenData, sizeof(screenData));
	});

	CascadeData cascades[4];
	CalculateCascades(cascades, sceneCamera, directionalLight.Direction);

	// TODO: four cascades for now
	for (int i = 0; i < 4; i++) {
		CascadeSplits[i] = cascades[i].SplitDepth;
		shadowData.ViewProjection[i] = cascades[i].ViewProj;
	}
	Renderer::submit([instance, shadowData]() mutable {
		const uint32_t bufferIndex = Renderer::get_current_frame_index();
		instance->m_UniformBufferSet->get(1, 0, bufferIndex)->rt_set_data(&shadowData, sizeof(shadowData));
	});

	rendererData.CascadeSplits = CascadeSplits;
	Renderer::submit([instance, rendererData]() mutable {
		const uint32_t bufferIndex = Renderer::get_current_frame_index();
		instance->m_UniformBufferSet->get(3, 0, bufferIndex)->rt_set_data(&rendererData, sizeof(rendererData));
	});

	Renderer::set_scene_environment(this, m_SceneData.SceneEnvironment, m_ShadowPassPipelines[0]->get_specification().RenderPass->get_specification().TargetFramebuffer->GetDepthImage());
}

void SceneRenderer::EndScene()
{

	CORE_ASSERT(m_Active);
#if MULTI_THREAD
	Reference<SceneRenderer> instance = this;
	s_ThreadPool.emplace_back(([instance]() mutable {
		instance->FlushDrawList();
	}));
#else
	FlushDrawList();
#endif

	m_Active = false;
}

void SceneRenderer::WaitForThreads()
{
	for (uint32_t i = 0; i < s_ThreadPool.size(); i++)
		s_ThreadPool[i].join();

	s_ThreadPool.clear();
}

void SceneRenderer::SubmitMesh(Reference<Mesh> mesh, uint32_t submeshIndex, Reference<MaterialTable> materialTable, const glm::mat4& transform, Reference<Material> overrideMaterial)
{

	// TODO: Culling, sorting, etc.

	const auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
	uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;
	Reference<MaterialAsset> material = materialTable->has_material(materialIndex) ? materialTable->get_material(materialIndex) : mesh->GetMaterials()->get_material(materialIndex);
	AssetHandle materialHandle = material->handle;

	MeshKey meshKey = { mesh->handle, materialHandle, submeshIndex, false };
	auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

	transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
	transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
	transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

	// Main geo
	{
		bool isTransparent = material->is_transparent();
		auto& destDrawList = !isTransparent ? m_DrawList : m_TransparentDrawList;
		auto& dc = destDrawList[meshKey];
		dc.Mesh = mesh;
		dc.SubmeshIndex = submeshIndex;
		dc.MaterialTable = materialTable;
		dc.OverrideMaterial = overrideMaterial;
		dc.InstanceCount++;
	}

	// Shadow pass
	if (material->is_shadow_casting()) {
		auto& dc = m_ShadowPassDrawList[meshKey];
		dc.Mesh = mesh;
		dc.SubmeshIndex = submeshIndex;
		dc.MaterialTable = materialTable;
		dc.OverrideMaterial = overrideMaterial;
		dc.InstanceCount++;
	}
}

void SceneRenderer::SubmitStaticMesh(Reference<StaticMesh> staticMesh, Reference<MaterialTable> materialTable, const glm::mat4& transform, Reference<Material> overrideMaterial)
{

	Reference<MeshSource> meshSource = staticMesh->GetMeshSource();
	const auto& submeshData = meshSource->GetSubmeshes();
	for (uint32_t submeshIndex : staticMesh->GetSubmeshes()) {
		glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

		const auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
		uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;
		Reference<MaterialAsset> material = materialTable->has_material(materialIndex) ? materialTable->get_material(materialIndex) : staticMesh->GetMaterials()->get_material(materialIndex);
		AssetHandle materialHandle = material->handle;

		MeshKey meshKey = { staticMesh->handle, materialHandle, submeshIndex, false };
		auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

		transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
		transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
		transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };

		// Main geo
		{
			bool isTransparent = material->is_transparent();
			auto& destDrawList = !isTransparent ? m_StaticMeshDrawList : m_TransparentStaticMeshDrawList;
			auto& dc = destDrawList[meshKey];
			dc.StaticMesh = staticMesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
		}

		// Shadow pass
		if (material->is_shadow_casting()) {
			auto& dc = m_StaticMeshShadowPassDrawList[meshKey];
			dc.StaticMesh = staticMesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
		}
	}
}

void SceneRenderer::SubmitSelectedMesh(Reference<Mesh> mesh, uint32_t submeshIndex, Reference<MaterialTable> materialTable, const glm::mat4& transform, Reference<Material> overrideMaterial)
{

	// TODO: Culling, sorting, etc.

	const auto& submeshes = mesh->GetMeshSource()->GetSubmeshes();
	uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;
	Reference<MaterialAsset> material = materialTable->has_material(materialIndex) ? materialTable->get_material(materialIndex) : mesh->GetMaterials()->get_material(materialIndex);
	AssetHandle materialHandle = material->handle;

	MeshKey meshKey = { mesh->handle, materialHandle, submeshIndex, true };
	auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

	transformStorage.MRow[0] = { transform[0][0], transform[1][0], transform[2][0], transform[3][0] };
	transformStorage.MRow[1] = { transform[0][1], transform[1][1], transform[2][1], transform[3][1] };
	transformStorage.MRow[2] = { transform[0][2], transform[1][2], transform[2][2], transform[3][2] };

	uint32_t instanceIndex = 0;

	// Main geo
	{
		bool isTransparent = material->is_transparent();
		auto& destDrawList = !isTransparent ? m_DrawList : m_TransparentDrawList;
		auto& dc = destDrawList[meshKey];
		dc.Mesh = mesh;
		dc.SubmeshIndex = submeshIndex;
		dc.MaterialTable = materialTable;
		dc.OverrideMaterial = overrideMaterial;

		instanceIndex = dc.InstanceCount;
		dc.InstanceCount++;
	}

	// Selected mesh list
	{
		auto& dc = m_SelectedMeshDrawList[meshKey];
		dc.Mesh = mesh;
		dc.SubmeshIndex = submeshIndex;
		dc.MaterialTable = materialTable;
		dc.OverrideMaterial = overrideMaterial;
		dc.InstanceCount++;
		dc.InstanceOffset = instanceIndex;
	}

	// Shadow pass
	if (material->is_shadow_casting()) {
		auto& dc = m_ShadowPassDrawList[meshKey];
		dc.Mesh = mesh;
		dc.SubmeshIndex = submeshIndex;
		dc.MaterialTable = materialTable;
		dc.OverrideMaterial = overrideMaterial;
		dc.InstanceCount++;
	}
}

void SceneRenderer::SubmitSelectedStaticMesh(Reference<StaticMesh> staticMesh, Reference<MaterialTable> materialTable, const glm::mat4& transform, Reference<Material> overrideMaterial)
{

	Reference<MeshSource> meshSource = staticMesh->GetMeshSource();
	const auto& submeshData = meshSource->GetSubmeshes();
	for (uint32_t submeshIndex : staticMesh->GetSubmeshes()) {
		glm::mat4 submeshTransform = transform * submeshData[submeshIndex].Transform;

		const auto& submeshes = staticMesh->GetMeshSource()->GetSubmeshes();
		uint32_t materialIndex = submeshes[submeshIndex].MaterialIndex;

		Reference<MaterialAsset> material = materialTable->has_material(materialIndex) ? materialTable->get_material(materialIndex) : staticMesh->GetMaterials()->get_material(materialIndex);
		AssetHandle materialHandle = material->handle;

		MeshKey meshKey = { staticMesh->handle, materialHandle, submeshIndex, true };
		auto& transformStorage = m_MeshTransformMap[meshKey].Transforms.emplace_back();

		transformStorage.MRow[0] = { submeshTransform[0][0], submeshTransform[1][0], submeshTransform[2][0], submeshTransform[3][0] };
		transformStorage.MRow[1] = { submeshTransform[0][1], submeshTransform[1][1], submeshTransform[2][1], submeshTransform[3][1] };
		transformStorage.MRow[2] = { submeshTransform[0][2], submeshTransform[1][2], submeshTransform[2][2], submeshTransform[3][2] };

		// Main geo
		{
			bool isTransparent = material->is_transparent();
			auto& destDrawList = !isTransparent ? m_StaticMeshDrawList : m_TransparentStaticMeshDrawList;
			auto& dc = destDrawList[meshKey];
			dc.StaticMesh = staticMesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
		}

		// Selected mesh list
		{
			auto& dc = m_SelectedStaticMeshDrawList[meshKey];
			dc.StaticMesh = staticMesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
		}

		// Shadow pass
		if (material->is_shadow_casting()) {
			auto& dc = m_StaticMeshShadowPassDrawList[meshKey];
			dc.StaticMesh = staticMesh;
			dc.SubmeshIndex = submeshIndex;
			dc.MaterialTable = materialTable;
			dc.OverrideMaterial = overrideMaterial;
			dc.InstanceCount++;
		}
	}
}

void SceneRenderer::SubmitPhysicsDebugMesh(Reference<Mesh> mesh, uint32_t submeshIndex, const glm::mat4& transform)
{
}

void SceneRenderer::SubmitPhysicsStaticDebugMesh(Reference<StaticMesh> staticMesh, const glm::mat4& transform, const bool isPrimitiveCollider)
{
}

void SceneRenderer::ClearPass(Reference<RenderPass> renderPass, bool explicitClear)
{
	Renderer::begin_render_pass(m_CommandBuffer, renderPass, explicitClear);
	Renderer::end_render_pass(m_CommandBuffer);
}

void SceneRenderer::ShadowMapPass()
{

	uint32_t frameIndex = Renderer::get_current_frame_index();

	auto& directionalLights = m_SceneData.SceneLightEnvironment.DirectionalLights;
	if (directionalLights[0].Intensity == 0.0f || !directionalLights[0].CastShadows) {
		// Clear shadow maps
		for (int i = 0; i < 4; i++)
			ClearPass(m_ShadowPassPipelines[i]->get_specification().RenderPass);

		return;
	}

	// TODO: change to four cascades (or set number)
	for (int i = 0; i < 4; i++) {
		Renderer::begin_render_pass(m_CommandBuffer, m_ShadowPassPipelines[i]->get_specification().RenderPass);

		// static glm::mat4 scaleBiasMatrix = glm::scale(glm::mat4(1.0f), { 0.5f, 0.5f, 0.5f }) * glm::translate(glm::mat4(1.0f), { 1, 1, 1 });

		// Render entities
		const Buffer cascade(&i, sizeof(uint32_t));
		for (auto& [mk, dc] : m_StaticMeshShadowPassDrawList) {
			CORE_VERIFY_BOOL(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_static_mesh_with_material(m_CommandBuffer, m_ShadowPassPipelines[i], m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_ShadowPassMaterial, cascade);
		}
		for (auto& [mk, dc] : m_ShadowPassDrawList) {
			CORE_VERIFY_BOOL(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_mesh_with_material(m_CommandBuffer, m_ShadowPassPipelines[i], m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_ShadowPassMaterial, cascade);
		}

		Renderer::end_render_pass(m_CommandBuffer);
	}
}

void SceneRenderer::PreDepthPass()
{

	uint32_t frameIndex = Renderer::get_current_frame_index();

	m_GPUTimeQueries.DepthPrePassQuery = m_CommandBuffer->BeginTimestampQuery();
	Renderer::begin_render_pass(m_CommandBuffer, m_PreDepthPipeline->get_specification().RenderPass);
	for (auto& [mk, dc] : m_StaticMeshDrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_static_mesh_with_material(m_CommandBuffer, m_PreDepthPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_PreDepthMaterial);
	}
	for (auto& [mk, dc] : m_DrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_mesh_with_material(m_CommandBuffer, m_PreDepthPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_PreDepthMaterial);
	}

	Renderer::end_render_pass(m_CommandBuffer);

	Renderer::begin_render_pass(m_CommandBuffer, m_PreDepthTransparentPipeline->get_specification().RenderPass);
#if 1
	for (auto& [mk, dc] : m_TransparentStaticMeshDrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_mesh_with_material(m_CommandBuffer, m_PreDepthTransparentPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_PreDepthMaterial);
	}
	for (auto& [mk, dc] : m_TransparentDrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_mesh_with_material(m_CommandBuffer, m_PreDepthTransparentPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_PreDepthMaterial);
	}
#endif

	Renderer::end_render_pass(m_CommandBuffer);
}

void SceneRenderer::HZBCompute()
{

	Reference<VulkanComputePipeline> pipeline = m_HierarchicalDepthPipeline.as<VulkanComputePipeline>();

	auto srcDepthImage = m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_depth_image().as<VulkanImage2D>();

	Renderer::submit([srcDepthImage, commandBuffer = m_CommandBuffer, hierarchicalZTex = m_HierarchicalDepthTexture.as<VulkanTexture2D>(), material = m_HierarchicalDepthMaterial.as<VulkanMaterial>(), pipeline]() mutable {
		const VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		Reference<VulkanImage2D> hierarchicalZImage = hierarchicalZTex->get_image(0).as<VulkanImage2D>();

		auto shader = material->get_shader().as<VulkanShader>();

		VkDescriptorSetLayout descriptorSetLayout = shader->get_descriptor_set_layout(0);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		constexpr uint32_t maxMipBatchSize = 4;
		const uint32_t hzbMipCount = hierarchicalZTex->get_mip_level_count();

		pipeline->begin(commandBuffer);

		auto ReduceHZB = [hzbMipCount, maxMipBatchSize, &shader, &hierarchicalZImage, &device, &pipeline, &allocInfo](const uint32_t startDestMip, const uint32_t parentMip, Reference<VulkanImage2D> parentImage, const glm::vec2& DispatchThreadIdToBufferUV, const glm::vec2& InputViewportMaxBound, const bool isFirstPass) {
			const VkDescriptorSet descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
			struct HierarchicalZComputePushConstants {
				glm::vec2 DispatchThreadIdToBufferUV;
				glm::vec2 InputViewportMaxBound;
				glm::vec2 InvSize;
				int FirstLod;
				bool IsFirstPass;
				char Padding[3] { 0, 0, 0 };
			} hierarchicalZComputePushConstants;

			hierarchicalZComputePushConstants.IsFirstPass = isFirstPass;
			hierarchicalZComputePushConstants.FirstLod = (int)startDestMip;
			hierarchicalZComputePushConstants.DispatchThreadIdToBufferUV = DispatchThreadIdToBufferUV;
			hierarchicalZComputePushConstants.InputViewportMaxBound = InputViewportMaxBound;

			std::array<VkWriteDescriptorSet, 5> writeDescriptors {};
			std::array<VkDescriptorImageInfo, 5> hzbImageInfos {};
			const uint32_t endDestMip = glm::min(startDestMip + maxMipBatchSize, hzbMipCount);
			uint32_t destMip;
			for (destMip = startDestMip; destMip < endDestMip; ++destMip) {
				uint32_t idx = destMip - startDestMip;
				hzbImageInfos[idx] = hierarchicalZImage->get_descriptor_info();
				hzbImageInfos[idx].imageView = hierarchicalZImage->rt_get_mip_image_view(destMip);
				hzbImageInfos[idx].sampler = VK_NULL_HANDLE;
			}

			// Fill the rest .. or we could enable the null descriptor feature
			destMip -= startDestMip;
			for (; destMip < maxMipBatchSize; ++destMip) {
				hzbImageInfos[destMip] = hierarchicalZImage->get_descriptor_info();
				hzbImageInfos[destMip].imageView = hierarchicalZImage->rt_get_mip_image_view(hzbMipCount - 1);
				hzbImageInfos[destMip].sampler = VK_NULL_HANDLE;
			}

			for (uint32_t i = 0; i < maxMipBatchSize; ++i) {
				writeDescriptors[i] = *shader->get_descriptor_set("o_HZB");
				writeDescriptors[i].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[i].dstArrayElement = i;
				writeDescriptors[i].pImageInfo = &hzbImageInfos[i];
			}

			hzbImageInfos[4] = parentImage->get_descriptor_info();
			hzbImageInfos[4].sampler = VulkanRenderer::get_point_sampler();

			writeDescriptors[4] = *shader->get_descriptor_set("u_InputDepth");
			writeDescriptors[4].dstSet = descriptorSet; // Should this be set inside the shader?
			writeDescriptors[4].pImageInfo = &hzbImageInfos[maxMipBatchSize];

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

			const glm::ivec2 srcSize { Math::DivideAndRoundUp(parentImage->get_size(), 1u << parentMip) };
			const glm::ivec2 dstSize = Math::DivideAndRoundUp(hierarchicalZImage->get_size(), 1u << startDestMip);
			hierarchicalZComputePushConstants.InvSize = glm::vec2 { 1.0f / (float)srcSize.x, 1.0f / (float)srcSize.y };

			pipeline->set_push_constants(&hierarchicalZComputePushConstants, sizeof(hierarchicalZComputePushConstants));

			pipeline->dispatch(descriptorSet, Math::DivideAndRoundUp(dstSize.x, 8), Math::DivideAndRoundUp(dstSize.y, 8), 1);
		};
		glm::ivec2 srcSize = srcDepthImage->get_size();

		// Reduce first 4 mips
		ReduceHZB(0, 0, srcDepthImage, { 1.0f / glm::vec2 { srcSize } }, { (glm::vec2 { srcSize } - 0.5f) / glm::vec2 { srcSize } }, true);
		// Reduce the next mips
		for (uint32_t startDestMip = maxMipBatchSize; startDestMip < hzbMipCount; startDestMip += maxMipBatchSize) {
			srcSize = Math::DivideAndRoundUp(hierarchicalZTex->get_size(), 1u << uint32_t(startDestMip - 1));
			ReduceHZB(startDestMip, startDestMip - 1, hierarchicalZImage, { 2.0f / glm::vec2 { srcSize } }, glm::vec2 { 1.0f }, false);
		}

		pipeline->end();
	});
}

void SceneRenderer::PreIntegration()
{

	Reference<VulkanComputePipeline> pipeline = m_PreIntegrationPipeline.as<VulkanComputePipeline>();

	glm::vec2 projectionParams = { m_SceneData.SceneCamera.Far, m_SceneData.SceneCamera.Near }; // Reversed
	Renderer::submit([projectionParams, hzbUVFactor = m_SSROptions.HZBUvFactor, depthImage = m_HierarchicalDepthTexture->get_image(0).as<VulkanImage2D>(), commandBuffer = m_CommandBuffer.as<VulkanRenderCommandBuffer>(),
						 visibilityTexture = m_VisibilityTexture, material = m_PreIntegrationMaterial.as<VulkanMaterial>(), pipeline]() mutable {
		const VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		Reference<VulkanImage2D> visibilityImage = visibilityTexture->get_image(0).as<VulkanImage2D>();

		struct PreIntegrationComputePushConstants {
			glm::vec2 HZBResFactor;
			glm::vec2 ResFactor;
			glm::vec2 ProjectionParams; //(x) = Near plane, (y) = Far plane
			int PrevLod = 0;
		} preIntegrationComputePushConstants;

		// Clear to 100% visibility .. TODO: this can be done once when the image is resized
		VkClearColorValue clearColor { { 1.f } };
		VkImageSubresourceRange subresourceRange {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.layerCount = 1;
		subresourceRange.levelCount = 1;
		vkCmdClearColorImage(commandBuffer->get_command_buffer(Renderer::get_current_frame_index()), visibilityImage->get_image_info().image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);

		std::array<VkWriteDescriptorSet, 3> writeDescriptors {};

		auto shader = material->get_shader().as<VulkanShader>();
		VkDescriptorSetLayout descriptorSetLayout = shader->get_descriptor_set_layout(0);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		pipeline->begin(commandBuffer);

		auto visibilityDescriptorImageInfo = visibilityImage->get_descriptor_info();
		auto depthDescriptorImageInfo = depthImage->get_descriptor_info();
		depthDescriptorImageInfo.sampler = VulkanRenderer::get_point_sampler();

		for (uint32_t i = 1; i < visibilityTexture->get_mip_level_count(); i++) {
			auto [mipWidth, mipHeight] = visibilityTexture->get_mip_size(i);
			const auto workGroupsX = (uint32_t)glm::ceil((float)mipWidth / 8.0f);
			const auto workGroupsY = (uint32_t)glm::ceil((float)mipHeight / 8.0f);

			// Output visibilityImage
			visibilityDescriptorImageInfo.imageView = visibilityImage->rt_get_mip_image_view(i);

			const VkDescriptorSet descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
			writeDescriptors[0] = *shader->get_descriptor_set("o_VisibilityImage");
			writeDescriptors[0].dstSet = descriptorSet;
			writeDescriptors[0].pImageInfo = &visibilityDescriptorImageInfo;

			// Input visibilityImage
			writeDescriptors[1] = *shader->get_descriptor_set("u_VisibilityTex");
			writeDescriptors[1].dstSet = descriptorSet;
			writeDescriptors[1].pImageInfo = &visibilityImage->get_descriptor_info();

			// Input HZB
			writeDescriptors[2] = *shader->get_descriptor_set("u_HZB");
			writeDescriptors[2].dstSet = descriptorSet;
			writeDescriptors[2].pImageInfo = &depthDescriptorImageInfo;

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

			auto [width, height] = visibilityTexture->get_mip_size(i);
			glm::vec2 resFactor = 1.0f / glm::vec2 { width, height };
			preIntegrationComputePushConstants.HZBResFactor = resFactor * hzbUVFactor;
			preIntegrationComputePushConstants.ResFactor = resFactor;
			preIntegrationComputePushConstants.ProjectionParams = projectionParams;
			preIntegrationComputePushConstants.PrevLod = (int)i - 1;

			pipeline->set_push_constants(&preIntegrationComputePushConstants, sizeof(preIntegrationComputePushConstants));
			pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
		}
		pipeline->end();
	});
}

void SceneRenderer::LightCullingPass()
{
	m_LightCullingMaterial->set("u_DepthMap", m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->GetDepthImage());

	Renderer::light_culling(m_CommandBuffer, m_LightCullingPipeline, m_UniformBufferSet, m_StorageBufferSet, m_LightCullingMaterial, m_LightCullingWorkGroups);
}

void SceneRenderer::GeometryPass()
{

	uint32_t frameIndex = Renderer::get_current_frame_index();

	Renderer::begin_render_pass(m_CommandBuffer, m_SelectedGeometryPipeline->get_specification().RenderPass);
	for (auto& [mk, dc] : m_SelectedStaticMeshDrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_static_mesh_with_material(m_CommandBuffer, m_SelectedGeometryPipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount, m_SelectedGeometryMaterial);
	}
	for (auto& [mk, dc] : m_SelectedMeshDrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_mesh_with_material(m_CommandBuffer, m_SelectedGeometryPipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount, m_SelectedGeometryMaterial);
	}
	Renderer::end_render_pass(m_CommandBuffer);

	Renderer::begin_render_pass(m_CommandBuffer, m_GeometryPipeline->get_specification().RenderPass);
	// Skybox
	m_SkyboxMaterial->set("u_Uniforms.TextureLod", m_SceneData.SkyboxLod);
	m_SkyboxMaterial->set("u_Uniforms.Intensity", m_SceneData.SceneEnvironmentIntensity);

	const Reference<TextureCube> radianceMap = m_SceneData.SceneEnvironment ? m_SceneData.SceneEnvironment->radiance_map : Renderer::get_white_texture(); // SOON BLACK!
	m_SkyboxMaterial->set("u_Texture", radianceMap);

	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_SkyboxPipeline, m_UniformBufferSet, nullptr, m_SkyboxMaterial);

	// Render static meshes

	for (auto& [mk, dc] : m_StaticMeshDrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		Renderer::render_static_mesh(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);
	}

	// Render dynamic meshes
	for (auto& [mk, dc] : m_DrawList) {
		const auto& transformData = m_MeshTransformMap.at(mk);
		// Renderer::RenderSubmesh(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), dc.Transform);
		Renderer::render_submesh_instanced(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);
	}

	{
		// Render static meshes
		for (auto& [mk, dc] : m_TransparentStaticMeshDrawList) {
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_static_mesh(m_CommandBuffer, m_TransparentGeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.StaticMesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.StaticMesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);
		}

		// Render dynamic meshes
		for (auto& [mk, dc] : m_TransparentDrawList) {
			const auto& transformData = m_MeshTransformMap.at(mk);
			// Renderer::RenderSubmesh(m_CommandBuffer, m_GeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), dc.Transform);
			Renderer::render_submesh_instanced(m_CommandBuffer, m_TransparentGeometryPipeline, m_UniformBufferSet, m_StorageBufferSet, dc.Mesh, dc.SubmeshIndex, dc.MaterialTable ? dc.MaterialTable : dc.Mesh->GetMaterials(), m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount);
		}
	}

	Renderer::end_render_pass(m_CommandBuffer);
}

void SceneRenderer::PreConvolutionCompute()
{

	// TODO: Other techniques might need it in the future
	if (!m_Options.EnableSSR)
		return;
	Reference<VulkanComputePipeline> pipeline = m_GaussianBlurPipeline.as<VulkanComputePipeline>();
	struct PreConvolutionComputePushConstants {
		int PrevLod = 0;
		int Mode = 0; // 0 = Copy, 1 = GaussianHorizontal, 2 = GaussianVertical
	} preConvolutionComputePushConstants;

	// Might change to be maximum res used by other techniques other than SSR.
	int halfRes = int(m_SSROptions.HalfRes);

	Renderer::submit([preConvolutionComputePushConstants, inputColorImage = m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0).as<VulkanImage2D>(),
						 preConvolutedTexture = m_PreConvolutedTexture.as<VulkanTexture2D>(), commandBuffer = m_CommandBuffer.as<VulkanRenderCommandBuffer>(),
						 material = m_GaussianBlurMaterial.as<VulkanMaterial>(), pipeline, halfRes]() mutable {
		const VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		Reference<VulkanImage2D> preConvolutedImage = preConvolutedTexture->get_image().as<VulkanImage2D>();

		auto shader = material->get_shader().as<VulkanShader>();

		VkDescriptorSetLayout descriptorSetLayout = shader->get_descriptor_set_layout(0);
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		std::array<VkWriteDescriptorSet, 2> writeDescriptors {};

		const VkDescriptorSet descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
		auto descriptorImageInfo = preConvolutedImage->get_descriptor_info();
		descriptorImageInfo.imageView = preConvolutedImage->rt_get_mip_image_view(0);
		descriptorImageInfo.sampler = VK_NULL_HANDLE;

		// Output Pre-convoluted image
		writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
		writeDescriptors[0].dstSet = descriptorSet;
		writeDescriptors[0].pImageInfo = &descriptorImageInfo;

		// Input original colors
		writeDescriptors[1] = *shader->get_descriptor_set("u_Input");
		writeDescriptors[1].dstSet = descriptorSet;
		writeDescriptors[1].pImageInfo = &inputColorImage->get_descriptor_info();

		uint32_t workGroupsX = (uint32_t)glm::ceil((float)inputColorImage->get_width() / 16.0f);
		uint32_t workGroupsY = (uint32_t)glm::ceil((float)inputColorImage->get_height() / 16.0f);

		pipeline->begin(commandBuffer);
		vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

		pipeline->set_push_constants(&preConvolutionComputePushConstants, sizeof preConvolutionComputePushConstants);
		pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);

		const uint32_t mipCount = preConvolutedTexture->get_mip_level_count();
		for (uint32_t mip = 1; mip < mipCount; mip++) {

			auto [mipWidth, mipHeight] = preConvolutedTexture->get_mip_size(mip);
			workGroupsX = (uint32_t)glm::ceil((float)mipWidth / 16.0f);
			workGroupsY = (uint32_t)glm::ceil((float)mipHeight / 16.0f);

			const VkDescriptorSet descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
			// Output image
			descriptorImageInfo.imageView = preConvolutedImage->rt_get_mip_image_view(mip);

			writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
			writeDescriptors[0].dstSet = descriptorSet;
			writeDescriptors[0].pImageInfo = &descriptorImageInfo;

			writeDescriptors[1] = *shader->get_descriptor_set("u_Input");
			writeDescriptors[1].dstSet = descriptorSet;
			writeDescriptors[1].pImageInfo = &preConvolutedImage->get_descriptor_info();

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);
			preConvolutionComputePushConstants.PrevLod = (int)mip - 1;

			auto blur = [&](const int mode) {
				preConvolutionComputePushConstants.Mode = (int)mode;
				pipeline->set_push_constants(&preConvolutionComputePushConstants, sizeof(preConvolutionComputePushConstants));
				pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
			};

			blur(1); // Horizontal blur
			blur(2); // Vertical Blur
		}
		pipeline->end();
	});
}

void SceneRenderer::DeinterleavingPass()
{
	if (!m_Options.EnableHBAO) {
		for (int i = 0; i < 2; i++)
			ClearPass(m_DeinterleavingPipelines[i]->get_specification().RenderPass);
		return;
	}

	m_DeinterleavingMaterial->set("u_Depth", m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->GetDepthImage());

	for (int i = 0; i < 2; i++) {
		Renderer::submit([i, material = m_DeinterleavingMaterial]() mutable {
			material->set("u_Info.UVOffsetIndex", i);
		});
		Renderer::begin_render_pass(m_CommandBuffer, m_DeinterleavingPipelines[i]->get_specification().RenderPass);
		Renderer::submit_fullscreen_quad(m_CommandBuffer, m_DeinterleavingPipelines[i], m_UniformBufferSet, nullptr, m_DeinterleavingMaterial);
		Renderer::end_render_pass(m_CommandBuffer);
	}
}

void SceneRenderer::HBAOCompute()
{
	if (!m_Options.EnableHBAO) {
		Renderer::clear_image(m_CommandBuffer, m_HBAOOutputImage);
		return;
	}
	m_HBAOMaterial->set("u_LinearDepthTexArray", m_DeinterleavingPipelines[0]->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0));
	m_HBAOMaterial->set("u_ViewNormalsMaskTex", m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(1));
	m_HBAOMaterial->set("o_Color", m_HBAOOutputImage);

	Renderer::dispatch_compute_shader(m_CommandBuffer, m_HBAOPipeline, m_UniformBufferSet, nullptr, m_HBAOMaterial, m_HBAOWorkGroups);
}

void SceneRenderer::GTAOCompute()
{
	m_GTAOMaterial->set("u_HiZDepth", m_HierarchicalDepthTexture);
	m_GTAOMaterial->set("u_HilbertLut", Renderer::get_hilbert_lut());
	m_GTAOMaterial->set("u_ViewNormal", m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(1));
	m_GTAOMaterial->set("o_AOwBentNormals", m_GTAOOutputImage);
	m_GTAOMaterial->set("o_Edges", m_GTAOEdgesOutputImage);

	const Buffer pushConstantBuffer(&GTAODataCB, sizeof GTAODataCB);

	Renderer::dispatch_compute_shader(m_CommandBuffer, m_GTAOPipeline, m_UniformBufferSet, nullptr, m_GTAOMaterial, m_GTAOWorkGroups, pushConstantBuffer);
}

void SceneRenderer::GTAODenoiseCompute()
{

	struct DenoisePushConstant {
		float DenoiseBlurBeta;
		bool HalfRes;
		char Padding1[3] { 0, 0, 0 };
	} denoisePushConstant;

	denoisePushConstant.DenoiseBlurBeta = GTAODataCB.DenoiseBlurBeta;
	denoisePushConstant.HalfRes = GTAODataCB.HalfRes;
	const Buffer pushConstantBuffer(&denoisePushConstant, sizeof denoisePushConstant);

	for (uint32_t pass = 0; pass < (uint32_t)m_Options.GTAODenoisePasses; ++pass)
		Renderer::dispatch_compute_shader(m_CommandBuffer, m_GTAODenoisePipeline, m_UniformBufferSet, nullptr, m_GTAODenoiseMaterial[uint32_t(pass % 2 != 0)], m_GTAODenoiseWorkGroups, pushConstantBuffer);
}

void SceneRenderer::ReinterleavingPass()
{
	if (!m_Options.EnableHBAO) {
		ClearPass(m_ReinterleavingPipeline->get_specification().RenderPass);
		return;
	}
	Renderer::begin_render_pass(m_CommandBuffer, m_ReinterleavingPipeline->get_specification().RenderPass);
	m_ReinterleavingMaterial->set("u_TexResultsArray", m_HBAOOutputImage);
	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_ReinterleavingPipeline, nullptr, nullptr, m_ReinterleavingMaterial);
	Renderer::end_render_pass(m_CommandBuffer);
}

void SceneRenderer::HBAOBlurPass()
{
	{
		Renderer::begin_render_pass(m_CommandBuffer, m_HBAOBlurPipelines[0]->get_specification().RenderPass);
		m_HBAOBlurMaterials[0]->set("u_Info.InvResDirection", glm::vec2 { m_InvViewportWidth, 0.0f });
		m_HBAOBlurMaterials[0]->set("u_Info.Sharpness", m_Options.HBAOBlurSharpness);
		Renderer::submit_fullscreen_quad(m_CommandBuffer, m_HBAOBlurPipelines[0], nullptr, nullptr, m_HBAOBlurMaterials[0]);
		Renderer::end_render_pass(m_CommandBuffer);
	}

	{
		Renderer::begin_render_pass(m_CommandBuffer, m_HBAOBlurPipelines[1]->get_specification().RenderPass);
		m_HBAOBlurMaterials[1]->set("u_Info.InvResDirection", glm::vec2 { 0.0f, m_InvViewportHeight });
		m_HBAOBlurMaterials[1]->set("u_Info.Sharpness", m_Options.HBAOBlurSharpness);
		Renderer::submit_fullscreen_quad(m_CommandBuffer, m_HBAOBlurPipelines[1], nullptr, nullptr, m_HBAOBlurMaterials[1]);
		Renderer::end_render_pass(m_CommandBuffer);
	}
}

void SceneRenderer::AOComposite()
{
	if (m_Options.EnableGTAO)
		m_AOCompositeMaterial->set("u_GTAOTex", m_GTAOFinalImage);
	if (m_Options.EnableHBAO)
		m_AOCompositeMaterial->set("u_HBAOTex", m_HBAOBlurPipelines[1]->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0));
	Renderer::begin_render_pass(m_CommandBuffer, m_AOCompositeRenderPass);
	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_AOCompositePipeline, nullptr, m_AOCompositeMaterial);
	Renderer::end_render_pass(m_CommandBuffer);
}

void SceneRenderer::JumpFloodPass()
{
	Renderer::begin_render_pass(m_CommandBuffer, m_JumpFloodInitPipeline->get_specification().RenderPass);

	auto framebuffer = m_SelectedGeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer;
	m_JumpFloodInitMaterial->set("u_Texture", framebuffer->get_image(0));

	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_JumpFloodInitPipeline, nullptr, m_JumpFloodInitMaterial);
	Renderer::end_render_pass(m_CommandBuffer);

	m_JumpFloodPassMaterial[0]->set("u_Texture", m_TempFramebuffers[0]->get_image(0));
	m_JumpFloodPassMaterial[1]->set("u_Texture", m_TempFramebuffers[1]->get_image(0));

	int steps = 2;
	int step = std::round(std::pow(steps - 1, 2));
	int index = 0;
	Buffer vertexOverrides;
	Reference<Framebuffer> passFB = m_JumpFloodPassPipeline[0]->get_specification().RenderPass->get_specification().TargetFramebuffer;
	glm::vec2 texelSize = { 1.0f / (float)passFB->get_width(), 1.0f / (float)passFB->get_height() };
	vertexOverrides.allocate(sizeof(glm::vec2) + sizeof(int));
	vertexOverrides.write(glm::value_ptr(texelSize), sizeof(glm::vec2));
	while (step != 0) {
		vertexOverrides.write(&step, sizeof(int), sizeof(glm::vec2));

		Renderer::begin_render_pass(m_CommandBuffer, m_JumpFloodPassPipeline[index]->get_specification().RenderPass);
		Renderer::submit_fullscreen_quad_with_overrides(m_CommandBuffer, m_JumpFloodPassPipeline[index], nullptr, m_JumpFloodPassMaterial[index], vertexOverrides, Buffer());
		Renderer::end_render_pass(m_CommandBuffer);

		index = (index + 1) % 2;
		step /= 2;
	}

	vertexOverrides.release();

	m_JumpFloodCompositeMaterial->set("u_Texture", m_TempFramebuffers[1]->get_image(0));
}

void SceneRenderer::SSRCompute()
{

	m_SSRMaterial->set("outColor", m_SSRImage);
	m_SSRMaterial->set("u_InputColor", m_PreConvolutedTexture);
	m_SSRMaterial->set("u_VisibilityBuffer", m_VisibilityTexture);
	m_SSRMaterial->set("u_HiZBuffer", m_HierarchicalDepthTexture);
	m_SSRMaterial->set("u_Normal", m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(1));
	m_SSRMaterial->set("u_MetalnessRoughness", m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(2));

	if ((int)m_Options.ReflectionOcclusionMethod & (int)ShaderDef::AOMethod::GTAO)
		m_SSRMaterial->set("u_GTAOTex", m_GTAOFinalImage);

	if ((int)m_Options.ReflectionOcclusionMethod & (int)ShaderDef::AOMethod::HBAO)
		m_SSRMaterial->set("u_HBAOTex", (m_HBAOBlurPipelines[1]->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0)));

	const Buffer pushConstantsBuffer(&m_SSROptions, sizeof m_SSROptions);

	Renderer::dispatch_compute_shader(m_CommandBuffer, m_SSRPipeline, m_UniformBufferSet, nullptr, m_SSRMaterial, m_SSRWorkGroups, pushConstantsBuffer);
}

void SceneRenderer::SSRCompositePass()
{
	// Currently scales the SSR, renders with transparency.
	// The alpha channel is the confidence.
	m_SSRCompositeMaterial->set("u_SSR", m_SSRImage);
	Renderer::begin_render_pass(m_CommandBuffer, m_SSRCompositePipeline->get_specification().RenderPass);
	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_SSRCompositePipeline, m_UniformBufferSet, m_SSRCompositeMaterial);
	Renderer::end_render_pass(m_CommandBuffer);
}

void SceneRenderer::BloomCompute()
{
	Reference<VulkanComputePipeline> pipeline = m_BloomComputePipeline.as<VulkanComputePipeline>();

	// m_BloomComputeMaterial->set("o_Image", m_BloomComputeTexture);

	struct BloomComputePushConstants {
		glm::vec4 Params;
		float LOD = 0.0f;
		int Mode = 0; // 0 = prefilter, 1 = downsample, 2 = firstUpsample, 3 = upsample
	} bloomComputePushConstants;
	bloomComputePushConstants.Params = { m_BloomSettings.Threshold, m_BloomSettings.Threshold - m_BloomSettings.Knee, m_BloomSettings.Knee * 2.0f, 0.25f / m_BloomSettings.Knee };
	bloomComputePushConstants.Mode = 0;

	auto inputImage = m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0).as<VulkanImage2D>();

	Renderer::submit([bloomComputePushConstants, inputImage, workGroupSize = m_BloomComputeWorkgroupSize, commandBuffer = m_CommandBuffer, bloomTextures = m_BloomComputeTextures, ubs = m_UniformBufferSet, material = m_BloomComputeMaterial.as<VulkanMaterial>(), pipeline]() mutable {
		constexpr bool useComputeQueue = false;

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		Reference<VulkanImage2D> images[3] = {
			bloomTextures[0]->get_image().as<VulkanImage2D>(),
			bloomTextures[1]->get_image().as<VulkanImage2D>(),
			bloomTextures[2]->get_image().as<VulkanImage2D>()
		};

		auto shader = material->get_shader().as<VulkanShader>();

		auto descriptorImageInfo = images[0]->get_descriptor_info();
		descriptorImageInfo.imageView = images[0]->rt_get_mip_image_view(0);

		std::array<VkWriteDescriptorSet, 3> writeDescriptors;

		VkDescriptorSetLayout descriptorSetLayout = shader->get_descriptor_set_layout(0);

		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descriptorSetLayout;

		pipeline->Begin(useComputeQueue ? nullptr : commandBuffer);

		if (false) {
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageMemoryBarrier.image = inputImage->get_image_info().image;
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, inputImage->get_specification().Mips, 0, 1 };
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				pipeline->get_active_command_buffer(),
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		// Output image
		VkDescriptorSet descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
		writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
		writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
		writeDescriptors[0].pImageInfo = &descriptorImageInfo;

		// Input image
		writeDescriptors[1] = *shader->get_descriptor_set("u_Texture");
		writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
		writeDescriptors[1].pImageInfo = &inputImage->get_descriptor_info();

		writeDescriptors[2] = *shader->get_descriptor_set("u_BloomTexture");
		writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
		writeDescriptors[2].pImageInfo = &inputImage->get_descriptor_info();

		vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

		uint32_t workGroupsX = bloomTextures[0]->get_width() / workGroupSize;
		uint32_t workGroupsY = bloomTextures[0]->get_height() / workGroupSize;

		pipeline->set_push_constants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
		pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);

		{
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.image = images[0]->get_image_info().image;
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[0]->get_specification().Mips, 0, 1 };
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				pipeline->get_active_command_buffer(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		bloomComputePushConstants.Mode = 1;

		uint32_t mips = bloomTextures[0]->get_mip_level_count() - 2;
		Renderer::RT_BeginGPUPerfMarker(commandBuffer, "Bloom-DownSample");
		for (uint32_t i = 1; i < mips; i++) {
			auto [mipWidth, mipHeight] = bloomTextures[0]->get_mip_size(i);
			workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
			workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);

			{
				// Output image
				descriptorImageInfo.imageView = images[1]->rt_get_mip_image_view(i);

				descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
				writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
				writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[0].pImageInfo = &descriptorImageInfo;

				// Input image
				writeDescriptors[1] = *shader->get_descriptor_set("u_Texture");
				writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
				auto descriptor = bloomTextures[0]->get_image(0).as<VulkanImage2D>()->get_descriptor_info();
				// descriptor.sampler = samplerClamp;
				writeDescriptors[1].pImageInfo = &descriptor;

				writeDescriptors[2] = *shader->get_descriptor_set("u_BloomTexture");
				writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[2].pImageInfo = &inputImage->get_descriptor_info();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

				bloomComputePushConstants.LOD = i - 1.0f;
				pipeline->set_push_constants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
				pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
			}

			{
				VkImageMemoryBarrier imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.image = images[1]->get_image_info().image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[1]->get_specification().Mips, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(
					pipeline->get_active_command_buffer(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}

			{
				descriptorImageInfo.imageView = images[0]->rt_get_mip_image_view(i);

				// Output image
				descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
				writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
				writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[0].pImageInfo = &descriptorImageInfo;

				// Input image
				writeDescriptors[1] = *shader->get_descriptor_set("u_Texture");
				writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
				auto descriptor = bloomTextures[1]->get_image(0).as<VulkanImage2D>()->get_descriptor_info();
				// descriptor.sampler = samplerClamp;
				writeDescriptors[1].pImageInfo = &descriptor;

				writeDescriptors[2] = *shader->get_descriptor_set("u_BloomTexture");
				writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
				writeDescriptors[2].pImageInfo = &inputImage->get_descriptor_info();

				vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

				bloomComputePushConstants.LOD = i;
				pipeline->set_push_constants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
				pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);
			}

			{
				VkImageMemoryBarrier imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.image = images[0]->get_image_info().image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[0]->get_specification().Mips, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(
					pipeline->get_active_command_buffer(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}
		}

		bloomComputePushConstants.Mode = 2;
		workGroupsX *= 2;
		workGroupsY *= 2;

		// Output image
		descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
		descriptorImageInfo.imageView = images[2]->rt_get_mip_image_view(mips - 2);

		writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
		writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
		writeDescriptors[0].pImageInfo = &descriptorImageInfo;

		// Input image
		writeDescriptors[1] = *shader->get_descriptor_set("u_Texture");
		writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
		writeDescriptors[1].pImageInfo = &bloomTextures[0]->get_image(0).as<VulkanImage2D>()->get_descriptor_info();

		writeDescriptors[2] = *shader->get_descriptor_set("u_BloomTexture");
		writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
		writeDescriptors[2].pImageInfo = &inputImage->get_descriptor_info();

		vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

		bloomComputePushConstants.LOD--;
		pipeline->set_push_constants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));

		auto [mipWidth, mipHeight] = bloomTextures[2]->get_mip_size(mips - 2);
		workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
		workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);
		pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);

		{
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageMemoryBarrier.image = images[2]->get_image_info().image;
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[2]->get_specification().Mips, 0, 1 };
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(
				pipeline->get_active_command_buffer(),
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &imageMemoryBarrier);
		}

		bloomComputePushConstants.Mode = 3;

		// Upsample
		for (int32_t mip = mips - 3; mip >= 0; mip--) {
			auto [mipWidth, mipHeight] = bloomTextures[2]->get_mip_size(mip);
			workGroupsX = (uint32_t)glm::ceil((float)mipWidth / (float)workGroupSize);
			workGroupsY = (uint32_t)glm::ceil((float)mipHeight / (float)workGroupSize);

			// Output image
			descriptorImageInfo.imageView = images[2]->rt_get_mip_image_view(mip);
			auto descriptorSet = VulkanRenderer::rt_allocate_descriptor_set(allocInfo);
			writeDescriptors[0] = *shader->get_descriptor_set("o_Image");
			writeDescriptors[0].dstSet = descriptorSet; // Should this be set inside the shader?
			writeDescriptors[0].pImageInfo = &descriptorImageInfo;

			// Input image
			writeDescriptors[1] = *shader->get_descriptor_set("u_Texture");
			writeDescriptors[1].dstSet = descriptorSet; // Should this be set inside the shader?
			writeDescriptors[1].pImageInfo = &bloomTextures[0]->get_image(0).as<VulkanImage2D>()->get_descriptor_info();

			writeDescriptors[2] = *shader->get_descriptor_set("u_BloomTexture");
			writeDescriptors[2].dstSet = descriptorSet; // Should this be set inside the shader?
			writeDescriptors[2].pImageInfo = &images[2]->get_descriptor_info();

			vkUpdateDescriptorSets(device, (uint32_t)writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

			bloomComputePushConstants.LOD = mip;
			pipeline->set_push_constants(&bloomComputePushConstants, sizeof(bloomComputePushConstants));
			pipeline->dispatch(descriptorSet, workGroupsX, workGroupsY, 1);

			{
				VkImageMemoryBarrier imageMemoryBarrier = {};
				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageMemoryBarrier.image = images[2]->get_image_info().image;
				imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, images[2]->get_specification().Mips, 0, 1 };
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				vkCmdPipelineBarrier(
					pipeline->get_active_command_buffer(),
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &imageMemoryBarrier);
			}
		}

		pipeline->end();
	});
}

void SceneRenderer::CompositePass()
{

	uint32_t frameIndex = Renderer::get_current_frame_index();

	m_GPUTimeQueries.CompositePassQuery = m_CommandBuffer->BeginTimestampQuery();

	Renderer::begin_render_pass(m_CommandBuffer, m_CompositePipeline->get_specification().RenderPass, true);

	auto framebuffer = m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer;
	float exposure = m_SceneData.SceneCamera.Camera.GetExposure();
	int textureSamples = framebuffer->get_specification().Samples;

	m_CompositeMaterial->set("u_Uniforms.Exposure", exposure);
	if (m_BloomSettings.Enabled) {
		m_CompositeMaterial->set("u_Uniforms.BloomIntensity", m_BloomSettings.Intensity);
		m_CompositeMaterial->set("u_Uniforms.BloomDirtIntensity", m_BloomSettings.DirtIntensity);
	} else {
		m_CompositeMaterial->set("u_Uniforms.BloomIntensity", 0.0f);
		m_CompositeMaterial->set("u_Uniforms.BloomDirtIntensity", 0.0f);
	}

	m_CompositeMaterial->set("u_Uniforms.Opacity", m_Opacity);

	// CompositeMaterial->set("u_Uniforms.TextureSamples", textureSamples);

	auto inputImage = m_GeometryPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
	m_CompositeMaterial->set("u_Texture", inputImage);
	m_CompositeMaterial->set("u_BloomTexture", m_BloomComputeTextures[2]);
	m_CompositeMaterial->set("u_BloomDirtTexture", m_BloomDirtTexture);
	m_CompositeMaterial->set("u_DepthTexture", m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->GetDepthImage());
	m_CompositeMaterial->set("u_TransparentDepthTexture", m_PreDepthTransparentPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->GetDepthImage());

	SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Composite");
	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_CompositePipeline, m_UniformBufferSet, m_CompositeMaterial);
	SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

	SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "JumpFlood-Composite");
	Renderer::submit_fullscreen_quad(m_CommandBuffer, m_JumpFloodCompositePipeline, nullptr, m_JumpFloodCompositeMaterial);
	SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

	Renderer::end_render_pass(m_CommandBuffer);

#if 0 // WIP
	  // DOF
		Renderer::begin_render_pass(m_CommandBuffer, m_DOFPipeline->get_specification().RenderPass);
		m_DOFMaterial->set("u_Texture", m_CompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0));
		m_DOFMaterial->set("u_DepthTexture", m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->GetDepthImage());
		Renderer::submit_fullscreen_quad(m_CommandBuffer, m_DOFPipeline, nullptr, m_DOFMaterial);
		Renderer::end_render_pass(m_CommandBuffer);
#endif

	// Renderer::begin_render_pass(m_CommandBuffer, m_JumpFloodCompositePipeline->get_specification().RenderPass);
	// Renderer::end_render_pass(m_CommandBuffer);

	// Grid
	if (GetOptions().ShowGrid) {
		Renderer::begin_render_pass(m_CommandBuffer, m_ExternalCompositeRenderPass);
		const static glm::mat4 transform = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(8.0f));
		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Grid");
		Renderer::render_quad(m_CommandBuffer, m_GridPipeline, m_UniformBufferSet, nullptr, m_GridMaterial, transform);
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);
		Renderer::end_render_pass(m_CommandBuffer);
	}

	if (m_Options.ShowSelectedInWireframe) {
		Renderer::begin_render_pass(m_CommandBuffer, m_ExternalCompositeRenderPass);

		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Static Meshes Wireframe");
		for (auto& [mk, dc] : m_SelectedStaticMeshDrawList) {
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_static_mesh_with_material(m_CommandBuffer, m_GeometryWireframePipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount, m_WireframeMaterial);
		}
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Dynamic Meshes Wireframe");
		for (auto& [mk, dc] : m_SelectedMeshDrawList) {
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_mesh_with_material(m_CommandBuffer, m_GeometryWireframePipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset + dc.InstanceOffset * sizeof(TransformVertexData), dc.InstanceCount, m_WireframeMaterial);
		}
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		Renderer::end_render_pass(m_CommandBuffer);
	}

	if (m_Options.ShowPhysicsColliders) {
		Renderer::begin_render_pass(m_CommandBuffer, m_ExternalCompositeRenderPass);
		auto pipeline = m_Options.ShowPhysicsCollidersOnTop ? m_GeometryWireframeOnTopPipeline : m_GeometryWireframePipeline;

		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Static Meshes Collider");
		for (auto& [mk, dc] : m_StaticColliderDrawList) {
			CORE_VERIFY_BOOL(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_static_mesh_with_material(m_CommandBuffer, pipeline, m_UniformBufferSet, nullptr, dc.StaticMesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, dc.OverrideMaterial);
		}
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		SceneRenderer::BeginGPUPerfMarker(m_CommandBuffer, "Dynamic Meshes Collider");
		for (auto& [mk, dc] : m_ColliderDrawList) {
			CORE_VERIFY_BOOL(m_MeshTransformMap.find(mk) != m_MeshTransformMap.end());
			const auto& transformData = m_MeshTransformMap.at(mk);
			Renderer::render_mesh_with_material(m_CommandBuffer, pipeline, m_UniformBufferSet, nullptr, dc.Mesh, dc.SubmeshIndex, m_SubmeshTransformBuffers[frameIndex].Buffer, transformData.TransformOffset, dc.InstanceCount, m_SimpleColliderMaterial);
		}
		SceneRenderer::EndGPUPerfMarker(m_CommandBuffer);

		Renderer::end_render_pass(m_CommandBuffer);
	}
}

void SceneRenderer::FlushDrawList()
{
	if (m_ResourcesCreated && m_ViewportWidth > 0 && m_ViewportHeight > 0) {
		// Reset GPU time queries
		m_GPUTimeQueries = SceneRenderer::GPUTimeQueries();

		PreRender();

		m_CommandBuffer->begin();

		// Main render passes
		ShadowMapPass();
		PreDepthPass();
		HZBCompute();
		PreIntegration();
		LightCullingPass();
		GeometryPass();

		// HBAO
		if (m_Options.EnableHBAO) {
			DeinterleavingPass();
			HBAOCompute();
			ReinterleavingPass();
			HBAOBlurPass();
		}
		// GTAO
		if (m_Options.EnableGTAO) {
			GTAOCompute();
			GTAODenoiseCompute();
		}

		if (m_Options.EnableGTAO || m_Options.EnableHBAO)
			AOComposite();

		PreConvolutionCompute();

		// Post-processing
		JumpFloodPass();

		// SSR
		if (m_Options.EnableSSR) {
			SSRCompute();
			SSRCompositePass();
		}

		BloomCompute();
		CompositePass();

		m_CommandBuffer->end();
		m_CommandBuffer->submit();
	} else {
		// Empty pass
		m_CommandBuffer->begin();

		ClearPass();

		m_CommandBuffer->end();
		m_CommandBuffer->submit();
	}

	UpdateStatistics();

	m_DrawList.clear();
	m_TransparentDrawList.clear();
	m_SelectedMeshDrawList.clear();
	m_ShadowPassDrawList.clear();

	m_StaticMeshDrawList.clear();
	m_TransparentStaticMeshDrawList.clear();
	m_SelectedStaticMeshDrawList.clear();
	m_StaticMeshShadowPassDrawList.clear();

	m_ColliderDrawList.clear();
	m_StaticColliderDrawList.clear();
	m_SceneData = {};

	m_MeshTransformMap.clear();
}

void SceneRenderer::PreRender()
{

	uint32_t frameIndex = Renderer::get_current_frame_index();

	uint32_t offset = 0;
	for (auto& [key, transformData] : m_MeshTransformMap) {
		transformData.TransformOffset = offset * sizeof(TransformVertexData);
		for (const auto& transform : transformData.Transforms) {
			m_SubmeshTransformBuffers[frameIndex].Data[offset] = transform;
			offset++;
		}
	}

	m_SubmeshTransformBuffers[frameIndex].Buffer->set_data(m_SubmeshTransformBuffers[frameIndex].Data, offset * sizeof(TransformVertexData));
}

void SceneRenderer::ClearPass()
{

	Renderer::begin_render_pass(m_CommandBuffer, m_CompositePipeline->get_specification().RenderPass, true);
	Renderer::end_render_pass(m_CommandBuffer);
}

Reference<RenderPass> SceneRenderer::GetFinalRenderPass()
{
	return m_CompositePipeline->get_specification().RenderPass;
}

Reference<Image2D> SceneRenderer::GetFinalPassImage()
{
	if (!m_ResourcesCreated)
		return nullptr;

	return m_CompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
}

SceneRendererOptions& SceneRenderer::GetOptions()
{
	return m_Options;
}

void SceneRenderer::CalculateCascades(CascadeData* cascades, const SceneRendererCamera& sceneCamera, const glm::vec3& lightDirection) const
{
	// TODO: Reversed Z projection?
	auto viewProjection = sceneCamera.Camera.get_reversed_projection_matrix() * sceneCamera.ViewMatrix;

	const int SHADOW_MAP_CASCADE_COUNT = 4;
	float cascadeSplits[SHADOW_MAP_CASCADE_COUNT];

	float nearClip = sceneCamera.Near;
	float farClip = sceneCamera.Far;
	float clipRange = farClip - nearClip;

	float minZ = nearClip;
	float maxZ = nearClip + clipRange;

	float range = maxZ - minZ;
	float ratio = maxZ / minZ;

	// Calculate split depths based on view camera frustum
	// Based on method presented in https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float p = (i + 1) / static_cast<float>(SHADOW_MAP_CASCADE_COUNT);
		float log = minZ * std::pow(ratio, p);
		float uniform = minZ + range * p;
		float d = CascadeSplitLambda * (log - uniform) + uniform;
		cascadeSplits[i] = (d - nearClip) / clipRange;
	}

	cascadeSplits[3] = 0.3f;

	// Manually set cascades here
	// cascadeSplits[0] = 0.05f;
	// cascadeSplits[1] = 0.15f;
	// cascadeSplits[2] = 0.3f;
	// cascadeSplits[3] = 1.0f;

	// Calculate orthographic projection matrix for each cascade
	float lastSplitDist = 0.0;
	for (uint32_t i = 0; i < SHADOW_MAP_CASCADE_COUNT; i++) {
		float splitDist = cascadeSplits[i];

		glm::vec3 frustumCorners[8] = {
			glm::vec3(-1.0f, 1.0f, -1.0f),
			glm::vec3(1.0f, 1.0f, -1.0f),
			glm::vec3(1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f, -1.0f, -1.0f),
			glm::vec3(-1.0f, 1.0f, 1.0f),
			glm::vec3(1.0f, 1.0f, 1.0f),
			glm::vec3(1.0f, -1.0f, 1.0f),
			glm::vec3(-1.0f, -1.0f, 1.0f),
		};

		// Project frustum corners into world space
		glm::mat4 invCam = glm::inverse(viewProjection);
		for (uint32_t i = 0; i < 8; i++) {
			glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[i], 1.0f);
			frustumCorners[i] = invCorner / invCorner.w;
		}

		for (uint32_t i = 0; i < 4; i++) {
			glm::vec3 dist = frustumCorners[i + 4] - frustumCorners[i];
			frustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist);
			frustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist);
		}

		// Get frustum center
		glm::vec3 frustumCenter = glm::vec3(0.0f);
		for (uint32_t i = 0; i < 8; i++)
			frustumCenter += frustumCorners[i];

		frustumCenter /= 8.0f;

		// frustumCenter *= 0.01f;

		float radius = 0.0f;
		for (uint32_t i = 0; i < 8; i++) {
			float distance = glm::length(frustumCorners[i] - frustumCenter);
			radius = glm::max(radius, distance);
		}
		radius = std::ceil(radius * 16.0f) / 16.0f;

		glm::vec3 maxExtents = glm::vec3(radius);
		glm::vec3 minExtents = -maxExtents;

		glm::vec3 lightDir = -lightDirection;
		glm::mat4 lightViewMatrix = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 0.0f, 1.0f));
		glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f + CascadeNearPlaneOffset, maxExtents.z - minExtents.z + CascadeFarPlaneOffset);

		// Offset to texel space to avoid shimmering (from https://stackoverflow.com/questions/33499053/cascaded-shadow-map-shimmering)
		glm::mat4 shadowMatrix = lightOrthoMatrix * lightViewMatrix;
		float ShadowMapResolution = m_ShadowPassPipelines[0]->get_specification().RenderPass->get_specification().TargetFramebuffer->get_width();
		glm::vec4 shadowOrigin = (shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) * ShadowMapResolution / 2.0f;
		glm::vec4 roundedOrigin = glm::round(shadowOrigin);
		glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
		roundOffset = roundOffset * 2.0f / ShadowMapResolution;
		roundOffset.z = 0.0f;
		roundOffset.w = 0.0f;

		lightOrthoMatrix[3] += roundOffset;

		// Store split distance and matrix in cascade
		cascades[i].SplitDepth = (nearClip + splitDist * clipRange) * -1.0f;
		cascades[i].ViewProj = lightOrthoMatrix * lightViewMatrix;
		cascades[i].View = lightViewMatrix;

		lastSplitDist = cascadeSplits[i];
	}
}

void SceneRenderer::UpdateStatistics()
{
}

void SceneRenderer::SetLineWidth(const float width)
{
	m_LineWidth = width;
	if (m_GeometryWireframePipeline)
		m_GeometryWireframePipeline->get_specification().LineWidth = width;
}

void SceneRenderer::OnImGuiRender()
{

	ImGui::Begin("Scene Renderer");

	ImGui::Text("Viewport Size: %d, %d", m_ViewportWidth, m_ViewportHeight);

	bool vsync = Application::the().get_window().is_vsync();
	if (ImGui::Checkbox("Vsync", &vsync))
		Application::the().get_window().set_vsync(vsync);

	ImGui::End();
}
}
