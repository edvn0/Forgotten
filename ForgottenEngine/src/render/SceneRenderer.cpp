//
// created by Edwin Carlsson on 2022-08-10.
//

#include "fg_pch.hpp"

#include "render/SceneRenderer.hpp"

#include "render/ComputePipeline.hpp"
#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Pipeline.hpp"
#include "render/RenderPass.hpp"
#include "render/Renderer.hpp"
#include "render/Renderer2D.hpp"
#include "render/RendererContext.hpp"
#include "render/Shader.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"

namespace ForgottenEngine {

	void SceneRenderer::init()
	{

		m_CommandBuffer = RenderCommandBuffer::create(0);

		uint32_t framesInFlight = Renderer::get_config().frames_in_flight;
		m_UniformBufferSet = UniformBufferSet::create(framesInFlight);
		m_UniformBufferSet->create(sizeof(UBCamera), 0);
		m_UniformBufferSet->create(sizeof(UBShadow), 1);
		m_UniformBufferSet->create(sizeof(UBScene), 2);
		m_UniformBufferSet->create(sizeof(UBRendererData), 3);
		m_UniformBufferSet->create(sizeof(UBPointLights), 4);
		m_UniformBufferSet->create(sizeof(UBScreenData), 17);
		m_UniformBufferSet->create(sizeof(UBSpotLights), 19);
		m_UniformBufferSet->create(sizeof(UBHBAOData), 18);
		m_UniformBufferSet->create(sizeof(UBSpotShadowData), 20);

		m_Renderer2D = Reference<Renderer2D>::create();

		m_CompositeShader = Renderer::get_shader_library()->get("SceneComposite");
		m_CompositeMaterial = Material::create(m_CompositeShader);

		// Light culling compute pipeline
		{
			m_StorageBufferSet = StorageBufferSet::create(framesInFlight);
			// m_StorageBufferSet->create(1, Binding::VisiblePointLightIndicesBuffer); // Can't allocate 0 bytes.. Resized later
			// m_StorageBufferSet->create(1, Binding::VisibleSpotLightIndicesBuffer); // Can't allocate 0 bytes.. Resized later

			m_LightCullingMaterial = Material::create(Renderer::get_shader_library()->get("LightCulling"), "LightCulling");
			Reference<Shader> lightCullingShader = Renderer::get_shader_library()->get("LightCulling");
			m_LightCullingPipeline = ComputePipeline::create(lightCullingShader);
		}

		VertexBufferLayout vertexLayout = {
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float3, "a_Tangent" },
			{ ShaderDataType::Float3, "a_Binormal" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		};

		VertexBufferLayout instanceLayout = {
			{ ShaderDataType::Float4, "a_MRow0" },
			{ ShaderDataType::Float4, "a_MRow1" },
			{ ShaderDataType::Float4, "a_MRow2" },
		};

		VertexBufferLayout boneInfluenceLayout = {
			{ ShaderDataType::Int4, "a_BoneIDs" },
			{ ShaderDataType::Float4, "a_BoneWeights" },
		};

		uint32_t shadowMapResolution = 4096;

		// Directional Shadow pass
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::DEPTH32F;
			spec.Usage = ImageUsage::Attachment;
			spec.Width = shadowMapResolution;
			spec.Height = shadowMapResolution;
			spec.Layers = 4; // 4 cascades
			spec.DebugName = "ShadowCascades";
			Reference<Image2D> cascadedDepthImage = Image2D::create(spec);
			cascadedDepthImage->invalidate();
			cascadedDepthImage->create_per_layer_image_views();

			FramebufferSpecification shadowMapFramebufferSpec;
			shadowMapFramebufferSpec.DebugName = "Dir Shadow Map";
			shadowMapFramebufferSpec.Width = shadowMapResolution;
			shadowMapFramebufferSpec.Height = shadowMapResolution;
			shadowMapFramebufferSpec.Attachments = { FramebufferTextureSpecification { ImageFormat::DEPTH32F } };
			shadowMapFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			shadowMapFramebufferSpec.DepthClearValue = 1.0f;
			shadowMapFramebufferSpec.NoResize = true;
			shadowMapFramebufferSpec.ExistingImage = cascadedDepthImage;

			auto shadowPassShader = Renderer::get_shader_library()->get("DirShadowMap");
			auto shadowPassShaderAnim = Renderer::get_shader_library()->get("DirShadowMap_Anim");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "DirShadowPass";
			pipelineSpec.Shader = shadowPassShader;
			pipelineSpec.DepthOperator = DepthCompareOperator::LessOrEqual;
			pipelineSpec.Layout = vertexLayout;
			pipelineSpec.InstanceLayout = instanceLayout;

			PipelineSpecification pipelineSpecAnim = pipelineSpec;
			pipelineSpecAnim.DebugName = "DirShadowPass-Anim";
			pipelineSpecAnim.Shader = shadowPassShaderAnim;

			// 4 cascades
			for (int i = 0; i < 4; i++) {
				shadowMapFramebufferSpec.ExistingImageLayers.clear();
				shadowMapFramebufferSpec.ExistingImageLayers.emplace_back(i);

				RenderPassSpecification shadowMapRenderPassSpec;
				shadowMapRenderPassSpec.DebugName = shadowMapFramebufferSpec.DebugName;
				shadowMapRenderPassSpec.TargetFramebuffer = Framebuffer::create(shadowMapFramebufferSpec);
				auto renderpass = RenderPass::create(shadowMapRenderPassSpec);

				pipelineSpec.RenderPass = renderpass;
				m_ShadowPassPipelines[i] = Pipeline::create(pipelineSpec);

				pipelineSpecAnim.RenderPass = renderpass;
				m_ShadowPassPipelinesAnim[i] = Pipeline::create(pipelineSpecAnim);
			}
			m_ShadowPassMaterial = Material::create(shadowPassShader, "DirShadowPass");
		}

		// Non-directional shadow mapping pass
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.Width = shadowMapResolution; // TODO(Yan): could probably halve these
			framebufferSpec.Height = shadowMapResolution;
			framebufferSpec.Attachments = { FramebufferTextureSpecification { ImageFormat::DEPTH32F } };
			framebufferSpec.DepthClearValue = 1.0f;
			framebufferSpec.NoResize = true;
			framebufferSpec.DebugName = "Spot Shadow Map";

			auto shadowPassShader = Renderer::get_shader_library()->get("SpotShadowMap");
			auto shadowPassShaderAnim = Renderer::get_shader_library()->get("SpotShadowMap_Anim");

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "SpotShadowPass";
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

			RenderPassSpecification shadowMapRenderPassSpec;
			shadowMapRenderPassSpec.TargetFramebuffer = Framebuffer::create(framebufferSpec);
			shadowMapRenderPassSpec.DebugName = "SpotShadowMap";
			pipelineSpec.RenderPass = RenderPass::create(shadowMapRenderPassSpec);

			PipelineSpecification pipelineSpecAnim = pipelineSpec;
			pipelineSpecAnim.DebugName = "SpotShadowPass-Anim";
			pipelineSpecAnim.Shader = shadowPassShaderAnim;

			m_SpotShadowPassPipeline = Pipeline::create(pipelineSpec);
			m_SpotShadowPassAnimPipeline = Pipeline::create(pipelineSpecAnim);

			m_SpotShadowPassMaterial = Material::create(shadowPassShader, "SpotShadowPass");
		}

		// PreDepth
		{
			FramebufferSpecification preDepthFramebufferSpec;
			preDepthFramebufferSpec.DebugName = "PreDepth-Opaque";
			// Linear depth, reversed device depth
			preDepthFramebufferSpec.Attachments = { /*ImageFormat::RED32F, */ ImageFormat::DEPTH32FSTENCIL8UINT };
			preDepthFramebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			preDepthFramebufferSpec.DepthClearValue = 0.0f;

			RenderPassSpecification preDepthRenderPassSpec;
			preDepthRenderPassSpec.DebugName = preDepthFramebufferSpec.DebugName;
			preDepthRenderPassSpec.TargetFramebuffer = Framebuffer::create(preDepthFramebufferSpec);

			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = preDepthFramebufferSpec.DebugName;

			pipelineSpec.Shader = Renderer::get_shader_library()->get("PreDepth");
			pipelineSpec.Layout = vertexLayout;
			pipelineSpec.InstanceLayout = instanceLayout;
			pipelineSpec.RenderPass = RenderPass::create(preDepthRenderPassSpec);
			m_PreDepthPipeline = Pipeline::create(pipelineSpec);
			m_PreDepthMaterial = Material::create(pipelineSpec.Shader, pipelineSpec.DebugName);

			pipelineSpec.DebugName = "PreDepth-Anim";
			pipelineSpec.Shader = Renderer::get_shader_library()->get("PreDepth_Anim");
			m_PreDepthPipelineAnim = Pipeline::create(pipelineSpec); // same renderpass as Predepth-Opaque

			pipelineSpec.DebugName = "PreDepth-Transparent";
			pipelineSpec.Shader = Renderer::get_shader_library()->get("PreDepth");
			preDepthFramebufferSpec.DebugName = pipelineSpec.DebugName;
			preDepthRenderPassSpec.TargetFramebuffer = Framebuffer::create(preDepthFramebufferSpec);
			preDepthRenderPassSpec.DebugName = pipelineSpec.DebugName;
			pipelineSpec.RenderPass = RenderPass::create(preDepthRenderPassSpec);
			m_PreDepthTransparentPipeline = Pipeline::create(pipelineSpec);

			// TODO(0x): Need PreDepth-Transparent-Animated pipeline
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

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = geoFramebufferSpec.DebugName;
			renderPassSpec.TargetFramebuffer = framebuffer;
			Reference<RenderPass> renderPass = RenderPass::create(renderPassSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "PBR-Static";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("HazelPBR_Static");
			pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
			pipelineSpecification.DepthWrite = false;
			pipelineSpecification.Layout = vertexLayout;
			pipelineSpecification.InstanceLayout = instanceLayout;
			pipelineSpecification.LineWidth = m_LineWidth;
			pipelineSpecification.RenderPass = renderPass;
			m_GeometryPipeline = Pipeline::create(pipelineSpecification);

			//
			// Transparent Geometry
			//
			pipelineSpecification.DebugName = "PBR-Transparent";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("HazelPBR_Transparent");
			pipelineSpecification.DepthOperator = DepthCompareOperator::GreaterOrEqual;
			m_TransparentGeometryPipeline = Pipeline::create(pipelineSpecification);

			//
			// Animated Geometry
			//
			pipelineSpecification.DebugName = "PBR-Anim";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("HazelPBR_Anim");
			pipelineSpecification.DepthOperator = DepthCompareOperator::Equal;
			m_GeometryPipelineAnim = Pipeline::create(pipelineSpecification);

			// TODO(0x): Need Transparent-Animated geometry pipeline
		}

		// Selected Geometry isolation (for outline pass)
		{
			FramebufferSpecification framebufferSpec;
			framebufferSpec.DebugName = "SelectedGeometry";
			framebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			framebufferSpec.Samples = 1;
			framebufferSpec.ClearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			framebufferSpec.DepthClearValue = 1.0f;

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = framebufferSpec.DebugName;
			renderPassSpec.TargetFramebuffer = Framebuffer::create(framebufferSpec);
			Reference<RenderPass> renderPass = RenderPass::create(renderPassSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = framebufferSpec.DebugName;
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("SelectedGeometry");
			pipelineSpecification.Layout = vertexLayout;
			pipelineSpecification.InstanceLayout = instanceLayout;
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.DepthOperator = DepthCompareOperator::LessOrEqual;
			m_SelectedGeometryPipeline = Pipeline::create(pipelineSpecification);
			m_SelectedGeometryMaterial = Material::create(pipelineSpecification.Shader, pipelineSpecification.DebugName);

			pipelineSpecification.DebugName = "SelectedGeometry-Anim";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("SelectedGeometry_Anim");
			m_SelectedGeometryPipelineAnim = Pipeline::create(pipelineSpecification); // Note: same framebuffer and renderpass as m_SelectedGeometryPipeline
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

			m_BloomDirtTexture = Renderer::get_white_texture(); // SHould be black
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
			m_DeinterleavingMaterial = Material::create(pipelineSpec.Shader, pipelineSpec.DebugName);
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
			ImageSpecification imageSpec;
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
			framebufferSpec.Attachments = { FramebufferTextureSpecification(ImageFormat::RGBA32F) };
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

			m_ReinterleavingMaterial = Material::create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
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
			compFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

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
			compFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
			compFramebufferSpec.Transfer = true;

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
			m_DOFMaterial = Material::create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
		}

		// External compositing
		{
			FramebufferSpecification extCompFramebufferSpec;
			extCompFramebufferSpec.DebugName = "External-Composite";
			extCompFramebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::DEPTH32FSTENCIL8UINT };
			extCompFramebufferSpec.ClearColor = { 0.5f, 0.1f, 0.1f, 1.0f };
			extCompFramebufferSpec.ClearColorOnLoad = false;
			extCompFramebufferSpec.ClearDepthOnLoad = false;
			// Use the color buffer from the final compositing pass, but the depth buffer from
			// the actual 3D geometry pass, in case we want to composite elements behind meshes
			// in the scene
			extCompFramebufferSpec.ExistingImages[0] = m_CompositePipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
			extCompFramebufferSpec.ExistingImages[1] = m_PreDepthPipeline->get_specification().RenderPass->get_specification().TargetFramebuffer->get_depth_image();
			Reference<Framebuffer> framebuffer = Framebuffer::create(extCompFramebufferSpec);

			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = extCompFramebufferSpec.DebugName;
			renderPassSpec.TargetFramebuffer = framebuffer;
			m_ExternalCompositeRenderPass = RenderPass::create(renderPassSpec);

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Wireframe";
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Wireframe = true;
			pipelineSpecification.DepthTest = true;
			pipelineSpecification.LineWidth = 2.0f;
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("Wireframe");
			pipelineSpecification.Layout = vertexLayout;
			pipelineSpecification.InstanceLayout = instanceLayout;
			pipelineSpecification.RenderPass = m_ExternalCompositeRenderPass;
			m_GeometryWireframePipeline = Pipeline::create(pipelineSpecification);

			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DebugName = "Wireframe-OnTop";
			m_GeometryWireframeOnTopPipeline = Pipeline::create(pipelineSpecification);

			pipelineSpecification.DepthTest = true;
			pipelineSpecification.DebugName = "Wireframe-Anim";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("Wireframe_Anim");
			m_GeometryWireframePipelineAnim = Pipeline::create(pipelineSpecification); // Note: same framebuffer and renderpass as m_GeometryWireframePipeline

			pipelineSpecification.DepthTest = false;
			pipelineSpecification.DebugName = "Wireframe-Anim-OnTop";
			m_GeometryWireframeOnTopPipelineAnim = Pipeline::create(pipelineSpecification);
		}

		// Read-back Image
		if (false) // WIP
		{
			ImageSpecification spec;
			spec.Format = ImageFormat::RGBA32F;
			spec.Usage = ImageUsage::HostRead;
			spec.Transfer = true;
			spec.DebugName = "ReadBack";
			m_ReadBackImage = Image2D::create(spec);
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
			RenderPassSpecification renderPassSpec;
			renderPassSpec.DebugName = "JumpFlood-Init";
			renderPassSpec.TargetFramebuffer = m_TempFramebuffers[0];

			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = renderPassSpec.DebugName;
			pipelineSpecification.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("JumpFlood_Init");
			m_JumpFloodInitPipeline = Pipeline::create(pipelineSpecification);
			m_JumpFloodInitMaterial = Material::create(pipelineSpecification.Shader, pipelineSpecification.DebugName);

			const char* passName[2] = { "EvenPass", "OddPass" };
			for (uint32_t i = 0; i < 2; i++) {
				renderPassSpec.TargetFramebuffer = m_TempFramebuffers[(i + 1) % 2];
				renderPassSpec.DebugName = fmt::format("JumpFlood-{0}", passName[i]);

				pipelineSpecification.DebugName = renderPassSpec.DebugName;
				pipelineSpecification.RenderPass = RenderPass::create(renderPassSpec);
				pipelineSpecification.Shader = Renderer::get_shader_library()->get("JumpFlood_Pass");
				m_JumpFloodPassPipeline[i] = Pipeline::create(pipelineSpecification);
				m_JumpFloodPassMaterial[i] = Material::create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
			}

			// Outline compositing
			{
				pipelineSpecification.RenderPass = m_CompositePipeline->get_specification().RenderPass;
				pipelineSpecification.DebugName = "JumpFlood-Composite";
				pipelineSpecification.Shader = Renderer::get_shader_library()->get("JumpFlood_Composite");
				pipelineSpecification.DepthTest = false;
				m_JumpFloodCompositePipeline = Pipeline::create(pipelineSpecification);
				m_JumpFloodCompositeMaterial = Material::create(pipelineSpecification.Shader, pipelineSpecification.DebugName);
			}
		}

		// Grid
		{
			PipelineSpecification pipelineSpec;
			pipelineSpec.DebugName = "Grid";
			pipelineSpec.Shader = Renderer::get_shader_library()->get("Grid");
			pipelineSpec.BackfaceCulling = false;
			pipelineSpec.Layout = {
				{ ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float2, "a_TexCoord" }
			};
			pipelineSpec.RenderPass = m_ExternalCompositeRenderPass;
			m_GridPipeline = Pipeline::create(pipelineSpec);

			const float gridScale = 16.025f;
			const float gridSize = 0.025f;
			m_GridMaterial = Material::create(pipelineSpec.Shader, pipelineSpec.DebugName);
			m_GridMaterial->set("u_Settings.Scale", gridScale);
			m_GridMaterial->set("u_Settings.Size", gridSize);
		}

		// Collider
		m_SimpleColliderMaterial = Material::create(Renderer::get_shader_library()->get("Wireframe"), "SimpleCollider");
		m_SimpleColliderMaterial->set("u_MaterialUniforms.Color", glm::vec4 { 0.3f, 0.9f, 0.9f, 1.0f });
		m_ComplexColliderMaterial = Material::create(Renderer::get_shader_library()->get("Wireframe"), "ComplexCollider");
		m_ComplexColliderMaterial->set("u_MaterialUniforms.Color", glm::vec4 { 0.3f, 0.9f, 0.9f, 1.0f });

		m_WireframeMaterial = Material::create(Renderer::get_shader_library()->get("Wireframe"), "Wireframe");
		m_WireframeMaterial->set("u_MaterialUniforms.Color", glm::vec4 { 1.0f, 0.5f, 0.0f, 1.0f });

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
			m_SkyboxMaterial = Material::create(pipelineSpec.Shader, pipelineSpec.DebugName);
			m_SkyboxMaterial->set_flag(MaterialFlag::DepthTest, false);
		}

		Renderer::submit([this]() {
			this->m_ResourcesCreated = true;
		});

		init_materials();
	}

	void SceneRenderer::init_materials()
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

	Reference<Pipeline> SceneRenderer::GetFinalPipeline()
	{
		return m_CompositePipeline;
	}

	Reference<RenderPass> SceneRenderer::GetFinalRenderPass()
	{
		return GetFinalPipeline()->get_specification().RenderPass;
	}

	Reference<RenderPass> SceneRenderer::GetCompositeRenderPass()
	{
		return m_CompositePipeline->get_specification().RenderPass;
	}

	Reference<RenderPass> SceneRenderer::GetExternalCompositeRenderPass()
	{
		return m_ExternalCompositeRenderPass;
	}

	Reference<Image2D> SceneRenderer::GetFinalPassImage()
	{
		if (!m_ResourcesCreated) {
			return nullptr;
		}
		return GetFinalPipeline()->get_specification().RenderPass->get_specification().TargetFramebuffer->get_image(0);
	}
} // namespace ForgottenEngine