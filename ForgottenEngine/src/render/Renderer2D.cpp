#include "fg_pch.hpp"

#include "render/Renderer2D.hpp"

#include "render/Font.hpp"
#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Mesh.hpp"
#include "render/Pipeline.hpp"
#include "render/RenderCommandBuffer.hpp"
#include "render/Renderer.hpp"
#include "render/StorageBuffer.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/UniformBuffer.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"

#include <codecvt>
#include <glm/gtc/matrix_transform.hpp>

// TEMP
#include "vulkan/VulkanRenderCommandBuffer.hpp"

namespace ForgottenEngine {

	Renderer2D::Renderer2D(const Renderer2DSpecification& specification)
		: specification(specification)
	{
		init();
	}

	Renderer2D::~Renderer2D() { shut_down(); }

	void Renderer2D::init()
	{
		if (specification.swap_chain_target)
			render_command_buffer = RenderCommandBuffer::create_from_swapchain();
		else
			render_command_buffer = RenderCommandBuffer::create(0);

		uint32_t framesInFlight = Renderer::get_config().frames_in_flight;

		FramebufferSpecification framebufferSpec;
		framebufferSpec.Attachments = { ImageFormat::RGBA32F, ImageFormat::Depth };
		framebufferSpec.Samples = 1;
		framebufferSpec.ClearColorOnLoad = true;
		framebufferSpec.ClearColor = { 0.1f, 0.9f, 0.5f, 1.0f };
		framebufferSpec.DebugName = "Renderer2D Framebuffer";

		Reference<Framebuffer> framebuffer = Framebuffer::create(framebufferSpec);

		RenderPassSpecification renderPassSpec;
		renderPassSpec.TargetFramebuffer = framebuffer;
		renderPassSpec.DebugName = "Renderer2D";
		Reference<RenderPass> renderPass = RenderPass::create(renderPassSpec);

		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Quad";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("Renderer2D");
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Layout = { { ShaderDataType::Float3, "a_Position" },
				{ ShaderDataType::Float4, "a_Color" }, { ShaderDataType::Float2, "a_TextureCoords" },
				{ ShaderDataType::Float, "a_TextureIndex" }, { ShaderDataType::Float, "a_TilingFactor" } };
			quad_pipeline = Pipeline::create(pipelineSpecification);

			quad_vertex_buffer.resize(framesInFlight);
			quad_vertex_buffer_base.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++) {
				quad_vertex_buffer[i] = VertexBuffer::create(MaxVertices * sizeof(QuadVertex));
				quad_vertex_buffer_base[i] = new QuadVertex[MaxVertices];
			}

			auto* quadIndices = new uint32_t[MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < MaxIndices; i += 6) {
				quadIndices[i + 0] = offset + 0;
				quadIndices[i + 1] = offset + 1;
				quadIndices[i + 2] = offset + 2;

				quadIndices[i + 3] = offset + 2;
				quadIndices[i + 4] = offset + 3;
				quadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			quad_index_buffer = IndexBuffer::create(quadIndices, MaxIndices);
			delete[] quadIndices;
		}

		white_texture = Renderer::get_white_texture();

		// set all texture slots to 0
		texture_slots[0] = white_texture;

		quad_vertex_positions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
		quad_vertex_positions[1] = { -0.5f, 0.5f, 0.0f, 1.0f };
		quad_vertex_positions[2] = { 0.5f, 0.5f, 0.0f, 1.0f };
		quad_vertex_positions[3] = { 0.5f, -0.5f, 0.0f, 1.0f };

		// Lines
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Line";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("Renderer2D_Line");
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.Topology = PrimitiveTopology::Lines;
			pipelineSpecification.LineWidth = 2.0f;
			pipelineSpecification.Layout
				= { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" } };
			line_pipeline = Pipeline::create(pipelineSpecification);
			pipelineSpecification.DepthTest = false;
			line_on_top_pipeline = Pipeline::create(pipelineSpecification);

			line_vertex_buffer.resize(framesInFlight);
			line_vertex_buffer_base.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++) {
				line_vertex_buffer[i] = VertexBuffer::create(MaxLineVertices * sizeof(LineVertex));
				line_vertex_buffer_base[i] = new LineVertex[MaxLineVertices];
			}

			auto* lineIndices = new uint32_t[MaxLineIndices];
			for (uint32_t i = 0; i < MaxLineIndices; i++)
				lineIndices[i] = i;

			line_index_buffer = IndexBuffer::create(lineIndices, MaxLineIndices);
			delete[] lineIndices;
		}

		// Text
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Text";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("Renderer2D_Text");
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.Layout
				= { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" },
					  { ShaderDataType::Float2, "a_TextureCoords" }, { ShaderDataType::Float, "a_TextureIndex" } };

			text_pipeline = Pipeline::create(pipelineSpecification);
			text_material = Material::create(pipelineSpecification.Shader);

			text_vertex_buffer.resize(framesInFlight);
			text_vertex_buffer_base.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++) {
				text_vertex_buffer[i] = VertexBuffer::create(MaxVertices * sizeof(TextVertex));
				text_vertex_buffer_base[i] = new TextVertex[MaxVertices];
			}

			auto* textQuadIndices = new uint32_t[MaxIndices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < MaxIndices; i += 6) {
				textQuadIndices[i + 0] = offset + 0;
				textQuadIndices[i + 1] = offset + 1;
				textQuadIndices[i + 2] = offset + 2;

				textQuadIndices[i + 3] = offset + 2;
				textQuadIndices[i + 4] = offset + 3;
				textQuadIndices[i + 5] = offset + 0;

				offset += 4;
			}

			text_index_buffer = IndexBuffer::create(textQuadIndices, MaxIndices);
			delete[] textQuadIndices;
		}

		// Circles
		{
			PipelineSpecification pipelineSpecification;
			pipelineSpecification.DebugName = "Renderer2D-Circle";
			pipelineSpecification.Shader = Renderer::get_shader_library()->get("Renderer2D_Circle");
			pipelineSpecification.BackfaceCulling = false;
			pipelineSpecification.RenderPass = renderPass;
			pipelineSpecification.Layout
				= { { ShaderDataType::Float3, "a_WorldPosition" }, { ShaderDataType::Float, "a_Thickness" },
					  { ShaderDataType::Float2, "a_LocalPosition" }, { ShaderDataType::Float4, "a_Color" } };
			circle_pipeline = Pipeline::create(pipelineSpecification);
			circle_material = Material::create(pipelineSpecification.Shader);

			circle_vertex_buffer.resize(framesInFlight);
			circle_vertex_buffer_base.resize(framesInFlight);
			for (uint32_t i = 0; i < framesInFlight; i++) {
				circle_vertex_buffer[i] = VertexBuffer::create(MaxVertices * sizeof(QuadVertex));
				circle_vertex_buffer_base[i] = new CircleVertex[MaxVertices];
			}
		}

		uniform_buffer_set = UniformBufferSet::create(framesInFlight);
		uniform_buffer_set->create(sizeof(UBCamera), 0);

		quad_material = Material::create(quad_pipeline->get_specification().Shader, "QuadMaterial");
		line_material = Material::create(line_pipeline->get_specification().Shader, "LineMaterial");
	}

	void Renderer2D::shut_down()
	{
		for (auto buffer : quad_vertex_buffer_base)
			delete[] buffer;

		for (auto buffer : text_vertex_buffer_base)
			delete[] buffer;

		for (auto buffer : line_vertex_buffer_base)
			delete[] buffer;

		for (auto buffer : circle_vertex_buffer_base)
			delete[] buffer;
	}

	void Renderer2D::begin_scene(const glm::mat4& viewProj, const glm::mat4& view, bool depthTest)
	{
		uint32_t frame_index = Renderer::get_current_frame_index();

		// Dirty shader impl
		// --------------------
		// end dirty shader impl
		camera_view_proj = viewProj;
		camera_view = view;
		depth_test = depthTest;

		Renderer::submit([ubs = uniform_buffer_set, viewProj]() mutable {
			uint32_t bufferIndex = Renderer::get_current_frame_index();
			auto ub = ubs->get(0, 0, bufferIndex);
			ub->rt_set_data(&viewProj, sizeof(UBCamera), 0);
		});

		quad_index_count = 0;
		quad_vertex_buffer_ptr = quad_vertex_buffer_base[frame_index];

		text_index_count = 0;
		text_vertex_buffer_ptr = text_vertex_buffer_base[frame_index];

		line_index_count = 0;
		line_vertex_buffer_ptr = line_vertex_buffer_base[frame_index];

		circle_index_count = 0;
		circle_vertex_buffer_ptr = circle_vertex_buffer_base[frame_index];

		texture_slot_index = 1;
		font_texture_slot_index = 0;

		for (uint32_t i = 1; i < texture_slots.size(); i++)
			texture_slots[i] = nullptr;

		for (auto& font_texture_slot : font_texture_slots)
			font_texture_slot = nullptr;
	}

	void Renderer2D::end_scene()
	{
		uint32_t frame_index = Renderer::get_current_frame_index();

		render_command_buffer->begin();
		Renderer::begin_render_pass(render_command_buffer, quad_pipeline->get_specification().RenderPass, false);

		auto data_size = (uint32_t)((uint8_t*)quad_vertex_buffer_ptr - (uint8_t*)quad_vertex_buffer_base[frame_index]);
		if (data_size) {
			quad_vertex_buffer[frame_index]->set_data(quad_vertex_buffer_base[frame_index], data_size);

			for (uint32_t i = 0; i < texture_slots.size(); i++) {
				if (texture_slots[i])
					quad_material->set("u_Textures", texture_slots[i], i);
				else
					quad_material->set("u_Textures", white_texture, i);
			}

			Renderer::render_geometry(render_command_buffer, quad_pipeline, uniform_buffer_set, nullptr, quad_material,
				quad_vertex_buffer[frame_index], quad_index_buffer, glm::mat4(1.0f), quad_index_count);

			stats.draw_calls++;
		}

		// Render text
		data_size = (uint32_t)((uint8_t*)text_vertex_buffer_ptr - (uint8_t*)text_vertex_buffer_base[frame_index]);
		if (data_size) {
			text_vertex_buffer[frame_index]->set_data(text_vertex_buffer_base[frame_index], data_size);

			for (uint32_t i = 0; i < font_texture_slots.size(); i++) {
				if (font_texture_slots[i])
					text_material->set("u_FontAtlases", font_texture_slots[i], i);
				else
					text_material->set("u_FontAtlases", white_texture, i);
			}

			Renderer::render_geometry(render_command_buffer, text_pipeline, uniform_buffer_set, nullptr, text_material,
				text_vertex_buffer[frame_index], text_index_buffer, glm::mat4(1.0f), text_index_count);

			stats.draw_calls++;
		}

		// Lines
		data_size = (uint32_t)((uint8_t*)line_vertex_buffer_ptr - (uint8_t*)line_vertex_buffer_base[frame_index]);
		if (data_size) {
			line_vertex_buffer[frame_index]->set_data(line_vertex_buffer_base[frame_index], data_size);

			Renderer::submit([lineWidth = line_width, renderCommandBuffer = render_command_buffer]() {
				uint32_t index = Renderer::get_current_frame_index();
				VkCommandBuffer commandBuffer
					= renderCommandBuffer.as<VulkanRenderCommandBuffer>()->get_command_buffer(index);
				vkCmdSetLineWidth(commandBuffer, lineWidth);
			});
			Renderer::render_geometry(render_command_buffer, line_pipeline, uniform_buffer_set, nullptr, line_material,
				line_vertex_buffer[frame_index], line_index_buffer, glm::mat4(1.0f), line_index_count);

			stats.draw_calls++;
		}

		// Circles
		data_size = (uint32_t)((uint8_t*)circle_vertex_buffer_ptr - (uint8_t*)circle_vertex_buffer_base[frame_index]);
		if (data_size) {
			circle_vertex_buffer[frame_index]->set_data(circle_vertex_buffer_base[frame_index], data_size);
			Renderer::render_geometry(render_command_buffer, circle_pipeline, uniform_buffer_set, nullptr,
				circle_material, circle_vertex_buffer[frame_index], quad_index_buffer, glm::mat4(1.0f),
				circle_index_count);

			stats.draw_calls++;
		}

		Renderer::end_render_pass(render_command_buffer);
		render_command_buffer->end();
		render_command_buffer->submit();
	}

	void Renderer2D::Flush()
	{
#if OLD
		// Bind textures
		for (uint32_t i = 0; i < texture_slot_index; i++)
			texture_slots[i]->Bind(i);

		m_QuadVertexArray->Bind();
		Renderer::DrawIndexed(quad_index_count, false);
		stats.draw_calls++;
#endif
	}

	Reference<RenderPass> Renderer2D::get_target_render_pass()
	{
		return quad_pipeline->get_specification().RenderPass;
	}

	void Renderer2D::set_target_render_pass(const Reference<RenderPass>& renderPass)
	{
		if (renderPass != quad_pipeline->get_specification().RenderPass) {
			{
				PipelineSpecification pipelineSpecification = quad_pipeline->get_specification();
				pipelineSpecification.RenderPass = renderPass;
				quad_pipeline = Pipeline::create(pipelineSpecification);
			}

			{
				PipelineSpecification pipelineSpecification = line_pipeline->get_specification();
				pipelineSpecification.RenderPass = renderPass;
				line_pipeline = Pipeline::create(pipelineSpecification);
			}

			{
				PipelineSpecification pipelineSpecification = text_pipeline->get_specification();
				pipelineSpecification.RenderPass = renderPass;
				text_pipeline = Pipeline::create(pipelineSpecification);
			}
		}
	}

	void Renderer2D::on_recreate_swapchain()
	{
		if (specification.swap_chain_target)
			render_command_buffer = RenderCommandBuffer::create_from_swapchain();
	}

	void Renderer2D::FlushAndReset()
	{
		uint32_t frame_index = Renderer::get_current_frame_index();
		// end_scene();

		quad_index_count = 0;
		quad_vertex_buffer_ptr = quad_vertex_buffer_base[frame_index];

		texture_slot_index = 1;
	}

	void Renderer2D::FlushAndResetLines() { }

	void Renderer2D::draw_quad(const glm::mat4& transform, const glm::vec4& color)
	{
		constexpr size_t quadVertexCount = 4;
		const float textureIndex = 0.0f; // White Texture
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		const float tilingFactor = 1.0f;

		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		for (size_t i = 0; i < quadVertexCount; i++) {
			quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[i];
			quad_vertex_buffer_ptr->Color = color;
			quad_vertex_buffer_ptr->TextureCoords = textureCoords[i];
			quad_vertex_buffer_ptr->TextureIndex = textureIndex;
			quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
			quad_vertex_buffer_ptr++;
		}

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad(const glm::mat4& transform, const Reference<Texture2D>& texture, float tilingFactor,
		const glm::vec4& tintColor)
	{
		constexpr size_t quadVertexCount = 4;
		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		constexpr glm::vec2 textureCoords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (texture_slots[i]->get_hash() == texture->get_hash()) {
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f) {
			if (texture_slot_index >= MaxTextureSlots)
				FlushAndReset();

			textureIndex = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		for (size_t i = 0; i < quadVertexCount; i++) {
			quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[i];
			quad_vertex_buffer_ptr->Color = color;
			quad_vertex_buffer_ptr->TextureCoords = textureCoords[i];
			quad_vertex_buffer_ptr->TextureIndex = textureIndex;
			quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
			quad_vertex_buffer_ptr++;
		}

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color)
	{
		draw_quad({ position.x, position.y, 0.0f }, size, color);
	}

	void Renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::mat4 transform
			= glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad(const glm::vec2& position, const glm::vec2& size, const Reference<Texture2D>& texture,
		float tilingFactor, const glm::vec4& tintColor)
	{
		draw_quad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::draw_quad(const glm::vec3& position, const glm::vec2& size, const Reference<Texture2D>& texture,
		float tilingFactor, const glm::vec4& tintColor)
	{
		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (*texture_slots[i].raw() == *texture.raw()) {
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f) {
			textureIndex = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		glm::mat4 transform
			= glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad_billboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::vec3 camRightWS = { camera_view[0][0], camera_view[1][0], camera_view[2][0] };
		glm::vec3 camUpWS = { camera_view[0][1], camera_view[1][1], camera_view[2][1] };

		quad_vertex_buffer_ptr->Position = position + camRightWS * (quad_vertex_positions[0].x) * size.x
			+ camUpWS * quad_vertex_positions[0].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = position + camRightWS * quad_vertex_positions[1].x * size.x
			+ camUpWS * quad_vertex_positions[1].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = position + camRightWS * quad_vertex_positions[2].x * size.x
			+ camUpWS * quad_vertex_positions[2].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = position + camRightWS * quad_vertex_positions[3].x * size.x
			+ camUpWS * quad_vertex_positions[3].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad_billboard(const glm::vec3& position, const glm::vec2& size,
		const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (texture_slots[i]->get_hash() == texture->get_hash()) {
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f) {
			textureIndex = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		glm::vec3 camRightWS = { camera_view[0][0], camera_view[1][0], camera_view[2][0] };
		glm::vec3 camUpWS = { camera_view[0][1], camera_view[1][1], camera_view[2][1] };

		quad_vertex_buffer_ptr->Position = position + camRightWS * (quad_vertex_positions[0].x) * size.x
			+ camUpWS * quad_vertex_positions[0].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = position + camRightWS * quad_vertex_positions[1].x * size.x
			+ camUpWS * quad_vertex_positions[1].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = position + camRightWS * quad_vertex_positions[2].x * size.x
			+ camUpWS * quad_vertex_positions[2].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = position + camRightWS * quad_vertex_positions[3].x * size.x
			+ camUpWS * quad_vertex_positions[3].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_rotated_quad(
		const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::draw_rotated_quad(
		const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		const float textureIndex = 0.0f; // White Texture
		const float tilingFactor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation,
		const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation,
		const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		if (quad_index_count >= MaxIndices)
			FlushAndReset();

		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float textureIndex = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (texture_slots[i]->get_hash() == texture->get_hash()) {
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f) {
			textureIndex = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = textureIndex;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_rotated_rect(
		const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		draw_rotated_rect({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::draw_rotated_rect(
		const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		if (line_index_count >= MaxLineIndices)
			FlushAndResetLines();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		glm::vec3 positions[4] = { transform * quad_vertex_positions[0], transform * quad_vertex_positions[1],
			transform * quad_vertex_positions[2], transform * quad_vertex_positions[3] };

		for (int i = 0; i < 4; i++) {
			auto& v0 = positions[i];
			auto& v1 = positions[(i + 1) % 4];

			line_vertex_buffer_ptr->Position = v0;
			line_vertex_buffer_ptr->Color = color;
			line_vertex_buffer_ptr++;

			line_vertex_buffer_ptr->Position = v1;
			line_vertex_buffer_ptr->Color = color;
			line_vertex_buffer_ptr++;

			line_index_count += 2;
			stats.line_count++;
		}
	}

	void Renderer2D::fill_circle(const glm::vec2& position, float radius, const glm::vec4& color, float thickness)
	{
		fill_circle({ position.x, position.y, 0.0f }, radius, color, thickness);
	}

	void Renderer2D::fill_circle(const glm::vec3& position, float radius, const glm::vec4& color, float thickness)
	{
		if (circle_index_count >= MaxIndices)
			FlushAndReset();

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::scale(glm::mat4(1.0f), { radius * 2.0f, radius * 2.0f, 1.0f });

		for (int i = 0; i < 4; i++) {
			circle_vertex_buffer_ptr->WorldPosition = transform * quad_vertex_positions[i];
			circle_vertex_buffer_ptr->Thickness = thickness;
			circle_vertex_buffer_ptr->LocalPosition = quad_vertex_positions[i] * 2.0f;
			circle_vertex_buffer_ptr->Color = color;
			circle_vertex_buffer_ptr++;

			circle_index_count += 6;
			stats.quad_count++;
		}
	}

	void Renderer2D::draw_line(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
	{
		if (line_index_count >= MaxLineIndices)
			FlushAndResetLines();

		line_vertex_buffer_ptr->Position = p0;
		line_vertex_buffer_ptr->Color = color;
		line_vertex_buffer_ptr++;

		line_vertex_buffer_ptr->Position = p1;
		line_vertex_buffer_ptr->Color = color;
		line_vertex_buffer_ptr++;

		line_index_count += 2;

		stats.line_count++;
	}

	void Renderer2D::draw_circle(
		const glm::vec3& position, const glm::vec3& rotation, float radius, const glm::vec4& color)
	{
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position)
			* glm::rotate(glm::mat4(1.0f), rotation.x, { 1.0f, 0.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.y, { 0.0f, 1.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.z, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), glm::vec3(radius));

		draw_circle(transform, color);
	}

	void Renderer2D::draw_circle(const glm::mat4& transform, const glm::vec4& color)
	{
		int segments = 32;
		for (int i = 0; i < segments; i++) {
			float angle = 2.0f * glm::pi<float>() * (float)i / segments;
			glm::vec4 startPosition = { glm::cos(angle), glm::sin(angle), 0.0f, 1.0f };
			angle = 2.0f * glm::pi<float>() * (float)((i + 1) % segments) / segments;
			glm::vec4 endPosition = { glm::cos(angle), glm::sin(angle), 0.0f, 1.0f };

			glm::vec3 p0 = transform * startPosition;
			glm::vec3 p1 = transform * endPosition;
			draw_line(p0, p1, color);
		}
	}

	void Renderer2D::draw_aabb(Reference<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color)
	{
		const auto& meshAssetSubmeshes = mesh->GetMeshSource()->GetSubmeshes();
		auto& submeshes = mesh->GetSubmeshes();
		for (uint32_t submeshIndex : submeshes) {
			const Submesh& submesh = meshAssetSubmeshes[submeshIndex];
			auto& aabb = submesh.BoundingBox;
			auto aabbTransform = transform * submesh.Transform;
			draw_aabb(aabb, aabbTransform);
		}
	}

	void Renderer2D::draw_aabb(
		const AABB& aabb, const glm::mat4& transform, const glm::vec4& color /*= glm::vec4(1.0f)*/)
	{
		glm::vec4 min = { aabb.min.x, aabb.min.y, aabb.min.z, 1.0f };
		glm::vec4 max = { aabb.max.x, aabb.max.y, aabb.max.z, 1.0f };

		glm::vec4 corners[8] = { transform * glm::vec4 { aabb.min.x, aabb.min.y, aabb.max.z, 1.0f },
			transform * glm::vec4 { aabb.min.x, aabb.max.y, aabb.max.z, 1.0f },
			transform * glm::vec4 { aabb.max.x, aabb.max.y, aabb.max.z, 1.0f },
			transform * glm::vec4 { aabb.max.x, aabb.min.y, aabb.max.z, 1.0f },

			transform * glm::vec4 { aabb.min.x, aabb.min.y, aabb.min.z, 1.0f },
			transform * glm::vec4 { aabb.min.x, aabb.max.y, aabb.min.z, 1.0f },
			transform * glm::vec4 { aabb.max.x, aabb.max.y, aabb.min.z, 1.0f },
			transform * glm::vec4 { aabb.max.x, aabb.min.y, aabb.min.z, 1.0f } };

		for (uint32_t i = 0; i < 4; i++)
			draw_line(corners[i], corners[(i + 1) % 4], color);

		for (uint32_t i = 0; i < 4; i++)
			draw_line(corners[i + 4], corners[((i + 1) % 4) + 4], color);

		for (uint32_t i = 0; i < 4; i++)
			draw_line(corners[i], corners[i + 4], color);
	}

	static bool NextLine(int index, const std::vector<int>& lines)
	{
		for (int line : lines) {
			if (line == index)
				return true;
		}
		return false;
	}

	void Renderer2D::draw_string(
		const std::string& string, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		// Use default font
		draw_string(string, Font::get_default_font(), position, maxWidth, color);
	}

	void Renderer2D::draw_string(const std::string& string, const Reference<Font>& font, const glm::vec3& position,
		float maxWidth, const glm::vec4& color)
	{
		draw_string(string, font, glm::translate(glm::mat4(1.0f), position), maxWidth, color);
	}

	// From https://stackoverflow.com/questions/31302506/stdu32string-conversion-to-from-stdstring-and-stdu16string
	static std::u32string To_UTF32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

	void Renderer2D::draw_string(const std::string& string, const Reference<Font>& font, const glm::mat4& transform,
		float maxWidth, const glm::vec4& color, float lineHeightOffset, float kerningOffset)
	{
		if (string.empty())
			return;

		float textureIndex = 0.0f;

		// TODO(Yan): this isn't really ideal, but we need to iterate through UTF-8 code points
		std::u32string utf32string = To_UTF32(string);

		Reference<Texture2D> font_atlas = font->get_font_atlas();
		CORE_ASSERT(font_atlas, "");

		for (uint32_t i = 0; i < font_texture_slot_index; i++) {
			if (*font_texture_slots[i].raw() == *font_atlas.raw()) {
				textureIndex = (float)i;
				break;
			}
		}

		if (textureIndex == 0.0f) {
			textureIndex = (float)font_texture_slot_index;
			font_texture_slots[font_texture_slot_index] = font_atlas;
			font_texture_slot_index++;
		}

		auto& fontGeometry = font->get_msdf_data()->FontGeometry;
		const auto& metrics = fontGeometry.getMetrics();

		// TODO(Yan): these font metrics really should be cleaned up/refactored...
		//            (this is a first pass WIP)
		std::vector<int> nextLines;
		{
			double x = 0.0;
			double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
			double y = -fsScale * metrics.ascenderY;
			int lastSpace = -1;
			for (int i = 0; i < utf32string.size(); i++) {
				char32_t character = utf32string[i];
				if (character == '\n') {
					x = 0;
					y -= fsScale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					glyph = fontGeometry.getGlyph('?');
				if (!glyph)
					continue;

				if (character != ' ') {
					// Calc geo
					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);
					glm::vec2 quad_min((float)pl, (float)pb);
					glm::vec2 quad_max((float)pr, (float)pt);

					quad_min *= fsScale;
					quad_max *= fsScale;
					quad_min += glm::vec2(x, y);
					quad_max += glm::vec2(x, y);

					if (quad_max.x > maxWidth && lastSpace != -1) {
						i = lastSpace;
						nextLines.emplace_back(lastSpace);
						lastSpace = -1;
						x = 0;
						y -= fsScale * metrics.lineHeight + lineHeightOffset;
					}
				} else {
					lastSpace = i;
				}

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, utf32string[i + 1]);
				x += fsScale * advance + kerningOffset;
			}
		}

		{
			double x = 0.0;
			double fsScale = 1 / (metrics.ascenderY - metrics.descenderY);
			double y = 0.0; // -fsScale * metrics.ascenderY;
			for (int i = 0; i < utf32string.size(); i++) {
				char32_t character = utf32string[i];
				if (character == '\n' || NextLine(i, nextLines)) {
					x = 0;
					y -= fsScale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				auto glyph = fontGeometry.getGlyph(character);
				if (!glyph)
					glyph = fontGeometry.getGlyph('?');
				if (!glyph)
					continue;

				double l, b, r, t;
				glyph->getQuadAtlasBounds(l, b, r, t);

				double pl, pb, pr, pt;
				glyph->getQuadPlaneBounds(pl, pb, pr, pt);

				pl *= fsScale, pb *= fsScale, pr *= fsScale, pt *= fsScale;
				pl += x, pb += y, pr += x, pt += y;

				double texelWidth = 1. / font_atlas->get_width();
				double texelHeight = 1. / font_atlas->get_height();
				l *= texelWidth, b *= texelHeight, r *= texelWidth, t *= texelHeight;

				// ImGui::begin("Font");
				// ImGui::Text("buffer_size: %d, %d", m_ExampleFontSheet->get_width(),
				// m_ExampleFontSheet->get_height()); UI::Image(m_ExampleFontSheet,
				// ImVec2(m_ExampleFontSheet->get_width(), m_ExampleFontSheet->get_height()), ImVec2(0, 1), ImVec2(1,
				// 0)); ImGui::End();

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pl, pb, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { l, b };
				text_vertex_buffer_ptr->TextureIndex = textureIndex;
				text_vertex_buffer_ptr++;

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pl, pt, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { l, t };
				text_vertex_buffer_ptr->TextureIndex = textureIndex;
				text_vertex_buffer_ptr++;

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pr, pt, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { r, t };
				text_vertex_buffer_ptr->TextureIndex = textureIndex;
				text_vertex_buffer_ptr++;

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pr, pb, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { r, b };
				text_vertex_buffer_ptr->TextureIndex = textureIndex;
				text_vertex_buffer_ptr++;

				text_index_count += 6;

				double advance = glyph->getAdvance();
				fontGeometry.getAdvance(advance, character, utf32string[i + 1]);
				x += fsScale * advance + kerningOffset;

				stats.quad_count++;
			}
		}
	}

	void Renderer2D::set_line_width(float lw) { line_width = lw; }

	void Renderer2D::reset_stats() { memset(&stats, 0, sizeof(Statistics)); }

	Renderer2D::Statistics Renderer2D::get_stats() { return stats; }

} // namespace ForgottenEngine
