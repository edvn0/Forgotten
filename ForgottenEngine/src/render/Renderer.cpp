#include "fg_pch.hpp"

#include "Application.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"

#include "render/IndexBuffer.hpp"
#include "render/Material.hpp"
#include "render/Pipeline.hpp"
#include "render/RenderPass.hpp"
#include "render/Shader.hpp"
#include "render/StorageBufferSet.hpp"
#include "render/Texture.hpp"
#include "render/UniformBufferSet.hpp"
#include "render/VertexBuffer.hpp"
#include "vulkan/VulkanRenderer.hpp"

#include "vulkan/VulkanSwapchain.hpp"

#include "render/RenderCommandQueue.hpp"

namespace ForgottenEngine {

struct RendererData {
public:
	size_t* temp{ nullptr };
	Reference<ShaderLibrary> shader_library;
	Reference<Texture2D> white_texture;
};

static RendererData* renderer_data;

static RendererAPI* renderer_api = nullptr;
static RenderCommandQueue resource_free_queue[3];
static RenderCommandQueue* command_queue = nullptr;
static RendererConfig config;

static RendererAPI* init_renderer_api()
{
	switch (RendererAPI::current()) {
	case RendererAPIType::Vulkan:
		return new VulkanRenderer();
	default:
		CORE_ASSERT(false, "Unknown RendererAPI");
	}
	return nullptr;
}

static inline auto& get() { return *renderer_data; }

void Renderer::init()
{
	CORE_DEBUG("Initializing renderer.");
	renderer_data = new RendererData();

	command_queue = new RenderCommandQueue();

	CORE_DEBUG("Initializing renderer API.");
	renderer_api = init_renderer_api();

	config.frames_in_flight = glm::min<uint32_t>(
		config.frames_in_flight, Application::the().get_window().get_swapchain().get_image_count());
	// Much stuff

	renderer_data->shader_library = Reference<ShaderLibrary>::create();

	// Load 2D Renderer Shaders.
	Renderer::get_shader_library()->load("shaders/2d_renderer");
	Renderer::get_shader_library()->load("shaders/2d_renderer_circle");
	Renderer::get_shader_library()->load("shaders/2d_renderer_line");
	Renderer::get_shader_library()->load("shaders/2d_renderer_text");

	Renderer::wait_and_render();

	renderer_api->init();
}

void Renderer::shut_down()
{
	renderer_api->shut_down();
	delete renderer_data;
	// Resource release queue
	for (uint32_t i = 0; i < config.frames_in_flight; i++) {
		auto& queue = Renderer::get_render_resource_free_queue(i);
		queue.execute();
	}

	delete command_queue;
}

void Renderer::wait_and_render() { command_queue->execute(); }

void Renderer::begin_frame() { }

void Renderer::begin_render_pass(
	const Reference<RenderCommandBuffer>& command_buffer, Reference<RenderPass> render_pass, bool explicit_clear)
{

	CORE_ASSERT(render_pass, "Render pass cannot be null!");

	renderer_api->begin_render_pass(command_buffer, render_pass, explicit_clear);
}

void Renderer::end_render_pass(const Reference<RenderCommandBuffer>& command_buffer)
{
	renderer_api->end_render_pass(command_buffer);
}

void Renderer::end_frame() { }

// Submits

void Renderer::render_geometry(const Reference<RenderCommandBuffer>& cmd_buffer,
	const Reference<Pipeline>& pipeline, const Reference<UniformBufferSet>& ubs,
	const Reference<StorageBufferSet>& sbs, const Reference<Material>& material, const Reference<VertexBuffer>& vb,
	const Reference<IndexBuffer>& ib, const glm::mat4& transform, uint32_t index_count)
{
	return renderer_api->render_geometry(cmd_buffer, pipeline, ubs, sbs, material, vb, ib, transform, index_count);
}

// End submits

RenderCommandQueue& Renderer::get_render_resource_free_queue(uint32_t index) { return resource_free_queue[index]; }

RenderCommandQueue& Renderer::get_render_command_queue() { return *command_queue; }

RendererConfig& Renderer::get_config() { return config; }

Reference<RendererContext> Renderer::get_context() { return Application::the().get_window().get_context(); }

Reference<ShaderLibrary> Renderer::get_shader_library() { return renderer_data->shader_library; }

uint32_t Renderer::get_current_frame_index()
{
	return Application::the().get_window().get_swapchain().get_current_buffer_index();
}

Reference<Texture2D> Renderer::get_white_texture() { return renderer_data->white_texture; }

} // namespace ForgottenEngine
