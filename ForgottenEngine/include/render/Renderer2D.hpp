#pragma once

#include "Common.hpp"

#include <glm/glm.hpp>

namespace ForgottenEngine {

	class RenderCommandBuffer;
	class Texture2D;
	class Pipeline;
	class Material;
	class RenderPass;
	class UniformBufferSet;
	class AABB;
	class Mesh;
	class Font;
	class IndexBuffer;
	class RenderCommandBuffer;
	class VertexBuffer;

	struct Renderer2DSpecification {
		bool swap_chain_target = true;
	};

	class Renderer2D : public ReferenceCounted {
	public:
		explicit Renderer2D(const Renderer2DSpecification& specification = Renderer2DSpecification());
		virtual ~Renderer2D();

		void init();
		void shut_down();

		void begin_scene(const glm::mat4& view_proj, const glm::mat4& view, bool depth_test = true);
		void end_scene();

		Reference<RenderPass> get_target_render_pass();
		void set_target_render_pass(const Reference<RenderPass>& render_pass);

		void on_recreate_swapchain();

		// Primitives
		void draw_quad(const glm::mat4& transform, const glm::vec4& color);
		void draw_quad(const glm::mat4& transform, const Reference<Texture2D>& texture, float tiling_factor = 1.0f,
			const glm::vec4& tint_color = glm::vec4(1.0f));

		void draw_quad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
		void draw_quad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void draw_quad(const glm::vec2& position, const glm::vec2& size, const Reference<Texture2D>& texture, float tiling_factor = 1.0f,
			const glm::vec4& tint_color = glm::vec4(1.0f));
		void draw_quad(const glm::vec3& position, const glm::vec2& size, const Reference<Texture2D>& texture, float tiling_factor = 1.0f,
			const glm::vec4& tint_color = glm::vec4(1.0f));

		void draw_quad_billboard(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
		void draw_quad_billboard(const glm::vec3& position, const glm::vec2& size, const Reference<Texture2D>& texture, float tiling_factor = 1.0f,
			const glm::vec4& tint_color = glm::vec4(1.0f));

		void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const glm::vec4& color);
		void draw_rotated_quad(const glm::vec2& position, const glm::vec2& size, float rotation, const Reference<Texture2D>& texture,
			float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));
		void draw_rotated_quad(const glm::vec3& position, const glm::vec2& size, float rotation, const Reference<Texture2D>& texture,
			float tiling_factor = 1.0f, const glm::vec4& tint_color = glm::vec4(1.0f));

		void draw_rotated_rect(const glm::vec2& position, const glm::vec2& size, float rot_radians, const glm::vec4& color);
		void draw_rotated_rect(const glm::vec3& position, const glm::vec2& size, float rot_radians, const glm::vec4& color);

		// Thickness is between 0 and 1
		void draw_circle(const glm::vec3& p0, const glm::vec3& rotation, float radius, const glm::vec4& color);
		void draw_circle(const glm::mat4& transform, const glm::vec4& color);
		void fill_circle(const glm::vec2& p0, float radius, const glm::vec4& color, float thickness = 0.05f);
		void fill_circle(const glm::vec3& p0, float radius, const glm::vec4& color, float thickness = 0.05f);

		void draw_line(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color = glm::vec4(1.0f));

		void draw_aabb(const AABB& aabb, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));
		void draw_aabb(Reference<Mesh> mesh, const glm::mat4& transform, const glm::vec4& color = glm::vec4(1.0f));

		void draw_string(const std::string& string, const glm::vec3& position, float max_width, const glm::vec4& color = glm::vec4(1.0f));
		void draw_string(const std::string& string, const Reference<Font>& font, const glm::vec3& position, float max_width,
			const glm::vec4& color = glm::vec4(1.0f));
		void draw_string(const std::string& string, const Reference<Font>& font, const glm::mat4& transform, float max_width,
			const glm::vec4& color = glm::vec4(1.0f), float line_height_offset = 0.0f, float kerning_offset = 0.0f);

		void set_line_width(float line_width);

		// Stats
		struct Statistics {
			uint32_t draw_calls = 0;
			uint32_t quad_count = 0;
			uint32_t line_count = 0;

			[[nodiscard]] uint32_t get_total_vertex_count() const { return quad_count * 4 + line_count * 2; }
			[[nodiscard]] uint32_t get_total_index_count() const { return quad_count * 6 + line_count * 2; }
		};
		void reset_stats();
		Statistics get_stats();

		Reference<Pipeline> composite_pipeline;
		Reference<Pipeline> pre_depth_pipeline;
		Reference<RenderPass> external_composite_render_pass;

	private:
		void flush();

		void flush_and_reset();
		void flush_and_reset_lines();

	private:
		struct QuadVertex {
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TextureCoords;
			float TextureIndex;
			float TilingFactor;
		};

		struct TextVertex {
			glm::vec3 Position;
			glm::vec4 Color;
			glm::vec2 TextureCoords;
			float TextureIndex;
		};

		struct LineVertex {
			glm::vec3 Position;
			glm::vec4 Color;
		};

		struct CircleVertex {
			glm::vec3 WorldPosition;
			float Thickness;
			glm::vec2 LocalPosition;
			glm::vec4 Color;
		};

		static constexpr uint32_t max_quads = 10000;
		static constexpr uint32_t max_vertices = max_quads * 4;
		static constexpr uint32_t max_indices = max_quads * 6;
		static constexpr uint32_t max_texture_slots = 16; // TODO: RenderCaps

		static constexpr uint32_t max_lines = 2000;
		static constexpr uint32_t max_line_vertices = max_lines * 2;
		static constexpr uint32_t max_line_indices = max_lines * 6;

		Renderer2DSpecification specification;
		Reference<RenderCommandBuffer> render_command_buffer;

		Reference<Texture2D> white_texture;

		Reference<Pipeline> quad_pipeline;
		std::vector<Reference<VertexBuffer>> quad_vertex_buffer;
		Reference<IndexBuffer> quad_index_buffer;
		Reference<Material> quad_material;

		uint32_t quad_index_count = 0;
		std::vector<QuadVertex*> quad_vertex_buffer_base;
		QuadVertex* quad_vertex_buffer_ptr;

		Reference<Pipeline> circle_pipeline;
		Reference<Material> circle_material;
		std::vector<Reference<VertexBuffer>> circle_vertex_buffer;
		uint32_t circle_index_count = 0;
		std::vector<CircleVertex*> circle_vertex_buffer_base;
		CircleVertex* circle_vertex_buffer_ptr;

		std::array<Reference<Texture2D>, max_texture_slots> texture_slots;
		uint32_t texture_slot_index = 1; // 0 = white texture

		glm::vec4 quad_vertex_positions[4];

		// Lines
		Reference<Pipeline> line_pipeline;
		Reference<Pipeline> line_on_top_pipeline;
		std::vector<Reference<VertexBuffer>> line_vertex_buffer;
		Reference<IndexBuffer> line_index_buffer;
		Reference<Material> line_material;

		uint32_t line_index_count = 0;
		std::vector<LineVertex*> line_vertex_buffer_base;
		LineVertex* line_vertex_buffer_ptr;

		// Text
		Reference<Pipeline> text_pipeline;
		std::vector<Reference<VertexBuffer>> text_vertex_buffer;
		Reference<IndexBuffer> text_index_buffer;
		Reference<Material> text_material;
		std::array<Reference<Texture2D>, max_texture_slots> font_texture_slots;
		uint32_t font_texture_slot_index = 0;

		uint32_t text_index_count = 0;
		std::vector<TextVertex*> text_vertex_buffer_base;
		TextVertex* text_vertex_buffer_ptr;

		glm::mat4 camera_view_proj;
		glm::mat4 camera_view;
		bool depth_test = true;

		float line_width = 1.0f;

		Statistics stats;

		Reference<UniformBufferSet> uniform_buffer_set;

		struct UBCamera {
			glm::mat4 ViewProjection;
		};
	};

} // namespace ForgottenEngine
