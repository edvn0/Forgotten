#pragma once

#include "Common.hpp"

namespace ForgottenEngine {

	class RenderPass;
	class RenderCommandBuffer;
	class Pipeline;
	class UniformBufferSet;
	class StorageBufferSet;
	class Material;
	class VertexBuffer;
	class IndexBuffer;

	enum class RendererAPIType { None,
		Vulkan };

	enum class PrimitiveType { None = 0,
		Triangles,
		Lines };

	class RendererAPI {
	public:
		virtual ~RendererAPI() = default;

		virtual void init() = 0;
		virtual void shut_down() = 0;

		virtual void begin_frame() = 0;
		virtual void begin_render_pass(
			Reference<RenderCommandBuffer> command_buffer, Reference<RenderPass> render_pass, bool explicit_clear)
			= 0;
		virtual void end_render_pass(Reference<RenderCommandBuffer> command_buffer) = 0;
		virtual void end_frame() = 0;

		// SUBMITS
		virtual void render_geometry(Reference<RenderCommandBuffer> command_buffer, Reference<Pipeline> pipeline,
			Reference<UniformBufferSet> ubs, Reference<StorageBufferSet> sbs, Reference<Material> material,
			Reference<VertexBuffer> vb, Reference<IndexBuffer> ib, const glm::mat4& transform, uint32_t index_count)
			= 0;

		virtual void submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer,
			const Reference<Pipeline>& pipeline_in, const Reference<UniformBufferSet>& ub,
			const Reference<StorageBufferSet>& sb, const Reference<Material>& material)
			= 0;

		virtual void submit_fullscreen_quad(const Reference<RenderCommandBuffer>& command_buffer,
			const Reference<Pipeline>& pipeline, const Reference<UniformBufferSet>& uniformBufferSet,
			const Reference<Material>& material)
			= 0;
		// END SUBMITS

	public:
		static RendererAPIType current() { return current_api; }
		static void set_api(RendererAPIType api)
		{
			CORE_ASSERT(api == RendererAPIType::Vulkan, "Must be Vulkan for now.");
			current_api = api;
		}

	private:
		inline static RendererAPIType current_api = RendererAPIType::Vulkan;
	};

} // namespace ForgottenEngine
