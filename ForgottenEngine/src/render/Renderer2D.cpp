#include "fg_pch.hpp"

#include "render/Renderer2D.hpp"

#include "render/Font.hpp"
#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Mesh.hpp"
#include "render/MSDFData.hpp"
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
		if (specification.swap_chain_target) {
			render_command_buffer = RenderCommandBuffer::create_from_swapchain();
		} else {
			render_command_buffer = RenderCommandBuffer::create(0);
		}

		uint32_t frames_in_flight = Renderer::get_config().frames_in_flight;

		FramebufferSpecification framebuffer_spec;
		framebuffer_spec.attachments = {
			ImageFormat::RGBA32F,
			ImageFormat::Depth,
		};
		framebuffer_spec.samples = 1;
		framebuffer_spec.clear_colour_on_load = true;
		framebuffer_spec.clear_colour = { 0.1f, 0.1f, 0.1f, 1.0f };
		framebuffer_spec.width = 1280;
		framebuffer_spec.height = 720;
		framebuffer_spec.debug_name = "Renderer2D Framebuffer";

		Reference<Framebuffer> framebuffer = Framebuffer::create(framebuffer_spec);

		RenderPassSpecification render_pass_spec;
		render_pass_spec.target_framebuffer = framebuffer;
		render_pass_spec.debug_name = "Renderer2D";
		Reference<RenderPass> render_pass = RenderPass::create(render_pass_spec);

		{
			PipelineSpecification pipeline_specification;
			pipeline_specification.debug_name = "Renderer2D-Quad";
			pipeline_specification.shader = Renderer::get_shader_library()->get("Renderer2D");
			pipeline_specification.render_pass = render_pass;
			pipeline_specification.backface_culling = false;
			pipeline_specification.layout
				= { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" }, { ShaderDataType::Float2, "a_TextureCoords" },
					  { ShaderDataType::Float, "a_TextureIndex" }, { ShaderDataType::Float, "a_TilingFactor" } };
			quad_pipeline = Pipeline::create(pipeline_specification);

			quad_vertex_buffer.resize(frames_in_flight);
			quad_vertex_buffer_base.resize(frames_in_flight);
			for (uint32_t i = 0; i < frames_in_flight; i++) {
				quad_vertex_buffer[i] = VertexBuffer::create(max_vertices * sizeof(QuadVertex));
				quad_vertex_buffer_base[i] = new QuadVertex[max_vertices];
			}

			auto* quad_indices = new uint32_t[max_indices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < max_indices; i += 6) {
				quad_indices[i + 0] = offset + 0;
				quad_indices[i + 1] = offset + 1;
				quad_indices[i + 2] = offset + 2;

				quad_indices[i + 3] = offset + 2;
				quad_indices[i + 4] = offset + 3;
				quad_indices[i + 5] = offset + 0;

				offset += 4;
			}

			quad_index_buffer = IndexBuffer::create(quad_indices, max_indices);
			delete[] quad_indices;
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
			PipelineSpecification pipeline_specification;
			pipeline_specification.debug_name = "Renderer2D-Line";
			pipeline_specification.shader = Renderer::get_shader_library()->get("Renderer2D_Line");
			pipeline_specification.render_pass = render_pass;
			pipeline_specification.topology = PrimitiveTopology::Lines;
			pipeline_specification.line_width = 2.0f;
			pipeline_specification.layout = { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" } };
			line_pipeline = Pipeline::create(pipeline_specification);
			pipeline_specification.depth_test = false;
			line_on_top_pipeline = Pipeline::create(pipeline_specification);

			line_vertex_buffer.resize(frames_in_flight);
			line_vertex_buffer_base.resize(frames_in_flight);
			for (uint32_t i = 0; i < frames_in_flight; i++) {
				line_vertex_buffer[i] = VertexBuffer::create(max_line_vertices * sizeof(LineVertex));
				line_vertex_buffer_base[i] = new LineVertex[max_line_vertices];
			}

			auto* line_indices = new uint32_t[max_line_indices];
			for (uint32_t i = 0; i < max_line_indices; i++) {
				line_indices[i] = i;
			}

			line_index_buffer = IndexBuffer::create(line_indices, max_line_indices);
			delete[] line_indices;
		}

		// Text
		{
			PipelineSpecification pipeline_specification;
			pipeline_specification.debug_name = "Renderer2D-Text";
			pipeline_specification.shader = Renderer::get_shader_library()->get("Renderer2D_Text");
			pipeline_specification.render_pass = render_pass;
			pipeline_specification.backface_culling = false;
			pipeline_specification.layout = { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float4, "a_Color" },
				{ ShaderDataType::Float2, "a_TextureCoords" }, { ShaderDataType::Float, "a_TextureIndex" } };

			text_pipeline = Pipeline::create(pipeline_specification);
			text_material = Material::create(pipeline_specification.shader);

			text_vertex_buffer.resize(frames_in_flight);
			text_vertex_buffer_base.resize(frames_in_flight);
			for (uint32_t i = 0; i < frames_in_flight; i++) {
				text_vertex_buffer[i] = VertexBuffer::create(max_vertices * sizeof(TextVertex));
				text_vertex_buffer_base[i] = new TextVertex[max_vertices];
			}

			auto* text_indices = new uint32_t[max_indices];

			uint32_t offset = 0;
			for (uint32_t i = 0; i < max_indices; i += 6) {
				text_indices[i + 0] = offset + 0;
				text_indices[i + 1] = offset + 1;
				text_indices[i + 2] = offset + 2;

				text_indices[i + 3] = offset + 2;
				text_indices[i + 4] = offset + 3;
				text_indices[i + 5] = offset + 0;

				offset += 4;
			}

			text_index_buffer = IndexBuffer::create(text_indices, max_indices);
			delete[] text_indices;
		}

		// Circles
		{
			PipelineSpecification pipeline_specification;
			pipeline_specification.debug_name = "Renderer2D-Circle";
			pipeline_specification.shader = Renderer::get_shader_library()->get("Renderer2D_Circle");
			pipeline_specification.backface_culling = false;
			pipeline_specification.render_pass = render_pass;
			pipeline_specification.layout = { { ShaderDataType::Float3, "a_WorldPosition" }, { ShaderDataType::Float, "a_Thickness" },
				{ ShaderDataType::Float2, "a_LocalPosition" }, { ShaderDataType::Float4, "a_Color" } };
			circle_pipeline = Pipeline::create(pipeline_specification);
			circle_material = Material::create(pipeline_specification.shader);

			circle_vertex_buffer.resize(frames_in_flight);
			circle_vertex_buffer_base.resize(frames_in_flight);
			for (uint32_t i = 0; i < frames_in_flight; i++) {
				circle_vertex_buffer[i] = VertexBuffer::create(max_vertices * sizeof(QuadVertex));
				circle_vertex_buffer_base[i] = new CircleVertex[max_vertices];
			}
		}

		VertexBufferLayout vertex_layout = { { ShaderDataType::Float3, "a_Position" }, { ShaderDataType::Float3, "a_Normal" },
			{ ShaderDataType::Float3, "a_Tangent" }, { ShaderDataType::Float3, "a_Binormal" }, { ShaderDataType::Float2, "a_TexCoord" } };

		VertexBufferLayout instance_layout = {
			{ ShaderDataType::Float4, "a_MRow0" },
			{ ShaderDataType::Float4, "a_MRow1" },
			{ ShaderDataType::Float4, "a_MRow2" },
		};

		VertexBufferLayout bone_influence_layout = {
			{ ShaderDataType::Int4, "a_BoneIDs" },
			{ ShaderDataType::Float4, "a_BoneWeights" },
		};

		Renderer::compile_shaders();

		uniform_buffer_set = UniformBufferSet::create(frames_in_flight);
		uniform_buffer_set->create(sizeof(UBCamera), 0);

		quad_material = Material::create(quad_pipeline->get_specification().shader, "QuadMaterial");
		line_material = Material::create(line_pipeline->get_specification().shader, "LineMaterial");
	}

	void Renderer2D::shut_down()
	{
		for (auto buffer : quad_vertex_buffer_base) {
			delete[] buffer;
		}

		for (auto buffer : text_vertex_buffer_base) {
			delete[] buffer;
		}

		for (auto buffer : line_vertex_buffer_base) {
			delete[] buffer;
		}

		for (auto buffer : circle_vertex_buffer_base) {
			delete[] buffer;
		}
	}

	void Renderer2D::begin_scene(const glm::mat4& view_proj, const glm::mat4& view, bool in_depth_test)
	{
		uint32_t frame_index = Renderer::get_current_frame_index();

		// Dirty shader impl
		// --------------------
		// end dirty shader impl
		camera_view_proj = view_proj;
		camera_view = view;
		depth_test = in_depth_test;

		Renderer::submit([ubs = uniform_buffer_set, view_proj]() mutable {
			uint32_t buffer_index = Renderer::get_current_frame_index();
			auto ub = ubs->get(0, 0, buffer_index);
			ub->render_thread_set_data(&view_proj, sizeof(UBCamera), 0);
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

		for (uint32_t i = 1; i < texture_slots.size(); i++) {
			texture_slots[i] = nullptr;
		}

		for (auto& font_texture_slot : font_texture_slots) {
			font_texture_slot = nullptr;
		}
	}

	void Renderer2D::end_scene()
	{
		uint32_t frame_index = Renderer::get_current_frame_index();

		render_command_buffer->begin();
		Renderer::begin_render_pass(render_command_buffer, quad_pipeline->get_specification().render_pass, false);

		auto data_size = (uint32_t)((uint8_t*)quad_vertex_buffer_ptr - (uint8_t*)quad_vertex_buffer_base[frame_index]);
		if (data_size) {
			quad_vertex_buffer[frame_index]->set_data(quad_vertex_buffer_base[frame_index], data_size);

			for (uint32_t i = 0; i < texture_slots.size(); i++) {
				if (texture_slots[i]) {
					quad_material->set("u_Textures", texture_slots[i], i);
				} else {
					quad_material->set("u_Textures", white_texture, i);
				}
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
				if (font_texture_slots[i]) {
					text_material->set("u_FontAtlases", font_texture_slots[i], i);
				} else {
					text_material->set("u_FontAtlases", white_texture, i);
				}
			}

			Renderer::render_geometry(render_command_buffer, text_pipeline, uniform_buffer_set, nullptr, text_material,
				text_vertex_buffer[frame_index], text_index_buffer, glm::mat4(1.0f), text_index_count);

			stats.draw_calls++;
		}

		// Lines
		data_size = (uint32_t)((uint8_t*)line_vertex_buffer_ptr - (uint8_t*)line_vertex_buffer_base[frame_index]);
		if (data_size) {
			line_vertex_buffer[frame_index]->set_data(line_vertex_buffer_base[frame_index], data_size);

			Renderer::submit([line_width = line_width, render_command_buffer = render_command_buffer]() {
				uint32_t index = Renderer::get_current_frame_index();
				VkCommandBuffer command_buffer = render_command_buffer.as<VulkanRenderCommandBuffer>()->get_command_buffer(index);
				vkCmdSetLineWidth(command_buffer, line_width);
			});
			Renderer::render_geometry(render_command_buffer, line_pipeline, uniform_buffer_set, nullptr, line_material,
				line_vertex_buffer[frame_index], line_index_buffer, glm::mat4(1.0f), line_index_count);

			stats.draw_calls++;
		}

		// Circles
		data_size = (uint32_t)((uint8_t*)circle_vertex_buffer_ptr - (uint8_t*)circle_vertex_buffer_base[frame_index]);
		if (data_size) {
			circle_vertex_buffer[frame_index]->set_data(circle_vertex_buffer_base[frame_index], data_size);
			Renderer::render_geometry(render_command_buffer, circle_pipeline, uniform_buffer_set, nullptr, circle_material,
				circle_vertex_buffer[frame_index], quad_index_buffer, glm::mat4(1.0f), circle_index_count);

			stats.draw_calls++;
		}

		Renderer::end_render_pass(render_command_buffer);
		render_command_buffer->end();
		render_command_buffer->submit();
	}

	void Renderer2D::flush() { }

	Reference<RenderPass> Renderer2D::get_target_render_pass() { return quad_pipeline->get_specification().render_pass; }

	void Renderer2D::set_target_render_pass(const Reference<RenderPass>& render_pass)
	{
		if (render_pass != quad_pipeline->get_specification().render_pass) {
			{
				PipelineSpecification pipeline_specification = quad_pipeline->get_specification();
				pipeline_specification.render_pass = render_pass;
				quad_pipeline = Pipeline::create(pipeline_specification);
			}

			{
				PipelineSpecification pipeline_specification = line_pipeline->get_specification();
				pipeline_specification.render_pass = render_pass;
				line_pipeline = Pipeline::create(pipeline_specification);
			}

			{
				PipelineSpecification pipeline_specification = text_pipeline->get_specification();
				pipeline_specification.render_pass = render_pass;
				text_pipeline = Pipeline::create(pipeline_specification);
			}
		}
	}

	void Renderer2D::on_recreate_swapchain()
	{
		if (specification.swap_chain_target) {
			render_command_buffer = RenderCommandBuffer::create_from_swapchain();
		}
	}

	void Renderer2D::flush_and_reset()
	{
		uint32_t frame_index = Renderer::get_current_frame_index();
		// end_scene();

		quad_index_count = 0;
		quad_vertex_buffer_ptr = quad_vertex_buffer_base[frame_index];

		texture_slot_index = 1;
	}

	void Renderer2D::flush_and_reset_lines() { }

	void Renderer2D::draw_quad(const glm::mat4& transform, const glm::vec4& color)
	{
		static constexpr size_t quad_vertex_count = 4;
		static const float texture_index = 0.0f; // White Texture
		static constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
		static const float tiling_factor = 1.0f;

		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		for (size_t i = 0; i < quad_vertex_count; i++) {
			quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[i];
			quad_vertex_buffer_ptr->Color = color;
			quad_vertex_buffer_ptr->TextureCoords = texture_coords[i];
			quad_vertex_buffer_ptr->TextureIndex = texture_index;
			quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
			quad_vertex_buffer_ptr++;
		}

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad(const glm::mat4& transform, const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		static constexpr size_t quad_vertex_count = 4;
		static constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		static constexpr glm::vec2 texture_coords[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		float texture_index = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (texture_slots[i]->get_hash() == texture->get_hash()) {
				texture_index = (float)i;
				break;
			}
		}

		if (texture_index == 0.0f) {
			if (texture_slot_index >= max_texture_slots) {
				flush_and_reset();
			}

			texture_index = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		for (size_t i = 0; i < quad_vertex_count; i++) {
			quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[i];
			quad_vertex_buffer_ptr->Color = color;
			quad_vertex_buffer_ptr->TextureCoords = texture_coords[i];
			quad_vertex_buffer_ptr->TextureIndex = texture_index;
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
		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		const float texture_index = 0.0f; // White Texture
		const float tiling_factor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad(
		const glm::vec2& position, const glm::vec2& size, const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		draw_quad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
	}

	void Renderer2D::draw_quad(
		const glm::vec3& position, const glm::vec2& size, const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float texture_index = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (*texture_slots[i].raw() == *texture.raw()) {
				texture_index = (float)i;
				break;
			}
		}

		if (texture_index == 0.0f) {
			texture_index = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad_billboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color)
	{
		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		const float texture_index = 0.0f; // White Texture
		const float tiling_factor = 1.0f;

		glm::vec3 cam_right_ws = { camera_view[0][0], camera_view[1][0], camera_view[2][0] };
		glm::vec3 cam_up_ws = { camera_view[0][1], camera_view[1][1], camera_view[2][1] };

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * (quad_vertex_positions[0].x) * size.x + cam_up_ws * quad_vertex_positions[0].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * quad_vertex_positions[1].x * size.x + cam_up_ws * quad_vertex_positions[1].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * quad_vertex_positions[2].x * size.x + cam_up_ws * quad_vertex_positions[2].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * quad_vertex_positions[3].x * size.x + cam_up_ws * quad_vertex_positions[3].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_quad_billboard(
		const glm::vec3& position, const glm::vec2& size, const Reference<Texture2D>& texture, float tilingFactor, const glm::vec4& tintColor)
	{
		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		constexpr glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float texture_index = 0.0f;
		for (uint32_t i = 1; i < texture_slot_index; i++) {
			if (texture_slots[i]->get_hash() == texture->get_hash()) {
				texture_index = (float)i;
				break;
			}
		}

		if (texture_index == 0.0f) {
			texture_index = (float)texture_slot_index;
			texture_slots[texture_slot_index] = texture;
			texture_slot_index++;
		}

		glm::vec3 cam_right_ws = { camera_view[0][0], camera_view[1][0], camera_view[2][0] };
		glm::vec3 cam_up_ws = { camera_view[0][1], camera_view[1][1], camera_view[2][1] };

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * (quad_vertex_positions[0].x) * size.x + cam_up_ws * quad_vertex_positions[0].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * quad_vertex_positions[1].x * size.x + cam_up_ws * quad_vertex_positions[1].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * quad_vertex_positions[2].x * size.x + cam_up_ws * quad_vertex_positions[2].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position
			= position + cam_right_ws * quad_vertex_positions[3].x * size.x + cam_up_ws * quad_vertex_positions[3].y * size.y;
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tilingFactor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

		static constexpr float texture_index = 0.0f; // White Texture
		static constexpr float tiling_factor = 1.0f;

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[0];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[1];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 0.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[2];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 1.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_vertex_buffer_ptr->Position = transform * quad_vertex_positions[3];
		quad_vertex_buffer_ptr->Color = color;
		quad_vertex_buffer_ptr->TextureCoords = { 0.0f, 1.0f };
		quad_vertex_buffer_ptr->TextureIndex = texture_index;
		quad_vertex_buffer_ptr->TilingFactor = tiling_factor;
		quad_vertex_buffer_ptr++;

		quad_index_count += 6;

		stats.quad_count++;
	}

	void Renderer2D::draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const Reference<Texture2D>& texture,
		float tilingFactor, const glm::vec4& tintColor)
	{
		draw_rotated_quad({ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor);
	}

	void Renderer2D::draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const Reference<Texture2D>& texture,
		float tilingFactor, const glm::vec4& tintColor)
	{
		if (quad_index_count >= max_indices) {
			flush_and_reset();
		}

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

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
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

	void Renderer2D::draw_rotated_rect(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		draw_rotated_rect({ position.x, position.y, 0.0f }, size, rotation, color);
	}

	void Renderer2D::draw_rotated_rect(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color)
	{
		if (line_index_count >= max_line_indices) {
			flush_and_reset_lines();
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation, { 0.0f, 0.0f, 1.0f })
			* glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });

		glm::vec3 positions[4] = { transform * quad_vertex_positions[0], transform * quad_vertex_positions[1], transform * quad_vertex_positions[2],
			transform * quad_vertex_positions[3] };

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
		if (circle_index_count >= max_indices) {
			flush_and_reset();
		}

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::scale(glm::mat4(1.0f), { radius * 2.0f, radius * 2.0f, 1.0f });

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
		if (line_index_count >= max_line_indices) {
			flush_and_reset_lines();
		}

		line_vertex_buffer_ptr->Position = p0;
		line_vertex_buffer_ptr->Color = color;
		line_vertex_buffer_ptr++;

		line_vertex_buffer_ptr->Position = p1;
		line_vertex_buffer_ptr->Color = color;
		line_vertex_buffer_ptr++;

		line_index_count += 2;

		stats.line_count++;
	}

	void Renderer2D::draw_circle(const glm::vec3& position, const glm::vec3& rotation, float radius, const glm::vec4& color)
	{
		const glm::mat4 transform = glm::translate(glm::mat4(1.0f), position) * glm::rotate(glm::mat4(1.0f), rotation.x, { 1.0f, 0.0f, 0.0f })
			* glm::rotate(glm::mat4(1.0f), rotation.y, { 0.0f, 1.0f, 0.0f }) * glm::rotate(glm::mat4(1.0f), rotation.z, { 0.0f, 0.0f, 1.0f })
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

	void Renderer2D::draw_aabb(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color /*= glm::vec4(1.0f)*/)
	{
		glm::vec4 min = { aabb.min.x, aabb.min.y, aabb.min.z, 1.0f };
		glm::vec4 max = { aabb.max.x, aabb.max.y, aabb.max.z, 1.0f };

		glm::vec4 corners[8] = { transform * glm::vec4 { aabb.min.x, aabb.min.y, aabb.max.z, 1.0f },
			transform * glm::vec4 { aabb.min.x, aabb.max.y, aabb.max.z, 1.0f }, transform * glm::vec4 { aabb.max.x, aabb.max.y, aabb.max.z, 1.0f },
			transform * glm::vec4 { aabb.max.x, aabb.min.y, aabb.max.z, 1.0f },

			transform * glm::vec4 { aabb.min.x, aabb.min.y, aabb.min.z, 1.0f }, transform * glm::vec4 { aabb.min.x, aabb.max.y, aabb.min.z, 1.0f },
			transform * glm::vec4 { aabb.max.x, aabb.max.y, aabb.min.z, 1.0f }, transform * glm::vec4 { aabb.max.x, aabb.min.y, aabb.min.z, 1.0f } };

		for (uint32_t i = 0; i < 4; i++) {
			draw_line(corners[i], corners[(i + 1) % 4], color);
		}

		for (uint32_t i = 0; i < 4; i++) {
			draw_line(corners[i + 4], corners[((i + 1) % 4) + 4], color);
		}

		for (uint32_t i = 0; i < 4; i++) {
			draw_line(corners[i], corners[i + 4], color);
		}
	}

	static bool NextLine(int index, const std::vector<int>& lines)
	{
		for (int line : lines) {
			if (line == index) {
				return true;
			}
		}
		return false;
	}

	void Renderer2D::draw_string(const std::string& string, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		// Use default font
		draw_string(string, Font::get_default_font(), position, maxWidth, color);
	}

	void Renderer2D::draw_string(
		const std::string& string, const Reference<Font>& font, const glm::vec3& position, float maxWidth, const glm::vec4& color)
	{
		draw_string(string, font, glm::translate(glm::mat4(1.0f), position), maxWidth, color);
	}

	// From https://stackoverflow.com/questions/31302506/stdu32string-conversion-to-from-stdstring-and-stdu16string
	static std::u32string to_utf_32(const std::string& s)
	{
		std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
		return conv.from_bytes(s);
	}

	void Renderer2D::draw_string(const std::string& string, const Reference<Font>& font, const glm::mat4& transform, float maxWidth,
		const glm::vec4& color, float lineHeightOffset, float kerningOffset)
	{
		if (string.empty()) {
			return;
		}

		float texture_index = 0.0f;

		// TODO(Yan): this isn't really ideal, but we need to iterate through UTF-8 code points
		std::u32string utf32string = to_utf_32(string);

		Reference<Texture2D> font_atlas = font->get_font_atlas();
		core_assert(font_atlas, "");

		for (uint32_t i = 0; i < font_texture_slot_index; i++) {
			if (*font_texture_slots[i].raw() == *font_atlas.raw()) {
				texture_index = (float)i;
				break;
			}
		}

		if (texture_index == 0.0f) {
			texture_index = (float)font_texture_slot_index;
			font_texture_slots[font_texture_slot_index] = font_atlas;
			font_texture_slot_index++;
		}

		auto& font_geometry = font->get_msdf_data()->font_geometry;
		const auto& metrics = font_geometry.getMetrics();

		// TODO(Yan): these font metrics really should be cleaned up/refactored...
		//            (this is a first pass WIP)
		std::vector<int> next_lines;
		{
			double x = 0.0;
			double fs_scale = 1 / (metrics.ascenderY - metrics.descenderY);
			double y = -fs_scale * metrics.ascenderY;
			int last_space = -1;
			for (int i = 0; i < utf32string.size(); i++) {
				char32_t character = utf32string[i];
				if (character == '\n') {
					x = 0;
					y -= fs_scale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				auto glyph = font_geometry.getGlyph(character);
				if (!glyph) {
					glyph = font_geometry.getGlyph('?');
				}
				if (!glyph) {
					continue;
				}

				if (character != ' ') {
					// Calc geo
					double pl, pb, pr, pt;
					glyph->getQuadPlaneBounds(pl, pb, pr, pt);
					glm::vec2 quad_min((float)pl, (float)pb);
					glm::vec2 quad_max((float)pr, (float)pt);

					quad_min *= fs_scale;
					quad_max *= fs_scale;
					quad_min += glm::vec2(x, y);
					quad_max += glm::vec2(x, y);

					if (quad_max.x > maxWidth && last_space != -1) {
						i = last_space;
						next_lines.emplace_back(last_space);
						last_space = -1;
						x = 0;
						y -= fs_scale * metrics.lineHeight + lineHeightOffset;
					}
				} else {
					last_space = i;
				}

				double advance = glyph->getAdvance();
				font_geometry.getAdvance(advance, character, utf32string[i + 1]);
				x += fs_scale * advance + kerningOffset;
			}
		}

		{
			double x = 0.0;
			double fs_scale = 1 / (metrics.ascenderY - metrics.descenderY);
			double y = 0.0; // -fsScale * metrics.ascenderY;
			for (int i = 0; i < utf32string.size(); i++) {
				char32_t character = utf32string[i];
				if (character == '\n' || NextLine(i, next_lines)) {
					x = 0;
					y -= fs_scale * metrics.lineHeight + lineHeightOffset;
					continue;
				}

				auto glyph = font_geometry.getGlyph(character);
				if (!glyph) {
					glyph = font_geometry.getGlyph('?');
				}
				if (!glyph) {
					continue;
				}

				double l, b, r, t;
				glyph->getQuadAtlasBounds(l, b, r, t);

				double pl, pb, pr, pt;
				glyph->getQuadPlaneBounds(pl, pb, pr, pt);

				pl *= fs_scale, pb *= fs_scale, pr *= fs_scale, pt *= fs_scale;
				pl += x, pb += y, pr += x, pt += y;

				double texel_width = 1. / font_atlas->get_width();
				double texel_height = 1. / font_atlas->get_height();
				l *= texel_width, b *= texel_height, r *= texel_width, t *= texel_height;

				// ImGui::begin("Font");
				// ImGui::Text("buffer_size: %d, %d", m_ExampleFontSheet->get_width(),
				// m_ExampleFontSheet->get_height()); UI::Image(m_ExampleFontSheet,
				// ImVec2(m_ExampleFontSheet->get_width(), m_ExampleFontSheet->get_height()), ImVec2(0, 1), ImVec2(1,
				// 0)); ImGui::End();

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pl, pb, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { l, b };
				text_vertex_buffer_ptr->TextureIndex = texture_index;
				text_vertex_buffer_ptr++;

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pl, pt, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { l, t };
				text_vertex_buffer_ptr->TextureIndex = texture_index;
				text_vertex_buffer_ptr++;

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pr, pt, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { r, t };
				text_vertex_buffer_ptr->TextureIndex = texture_index;
				text_vertex_buffer_ptr++;

				text_vertex_buffer_ptr->Position = transform * glm::vec4(pr, pb, 0.0f, 1.0f);
				text_vertex_buffer_ptr->Color = color;
				text_vertex_buffer_ptr->TextureCoords = { r, b };
				text_vertex_buffer_ptr->TextureIndex = texture_index;
				text_vertex_buffer_ptr++;

				text_index_count += 6;

				double advance = glyph->getAdvance();
				font_geometry.getAdvance(advance, character, utf32string[i + 1]);
				x += fs_scale * advance + kerningOffset;

				stats.quad_count++;
			}
		}
	}

	void Renderer2D::set_line_width(float lw) { line_width = lw; }

	void Renderer2D::reset_stats() { memset(&stats, 0, sizeof(Statistics)); }

	Renderer2D::Statistics Renderer2D::get_stats() { return stats; }

} // namespace ForgottenEngine
