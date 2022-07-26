#include "fg_pch.hpp"

#include "Application.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"

#include "vulkan/VulkanRenderer.hpp"

#include "vulkan/VulkanSwapchain.hpp"

#include "render/RenderCommandQueue.hpp"

namespace ForgottenEngine {

struct RendererData {
public:
	size_t* temp;
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

	renderer_api->init();
}

void Renderer::shut_down()
{
	delete renderer_data;

	renderer_api->shut_down();
	delete renderer_api;
}

void Renderer::wait_and_render() { command_queue->execute(); }

void Renderer::begin_frame() { }

void Renderer::end_frame() { }

RenderCommandQueue& Renderer::get_render_resource_free_queue(uint32_t index) { return resource_free_queue[index]; }

RenderCommandQueue& Renderer::get_render_command_queue() { return *command_queue; }

RendererConfig& Renderer::get_config() { return config; }

Reference<RendererContext> Renderer::get_context() { return Application::the().get_window().get_context(); }

} // namespace ForgottenEngine
