#include "fg_pch.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"

#include "vulkan/VulkanRenderer.hpp"

#include "render/RenderCommandQueue.hpp"

namespace ForgottenEngine {

struct RendererData {
public:
	size_t* temp;

public:
	~RendererData()
	{
		CORE_DEBUG("Releasing Renderer Data.");
		delete[] temp;
	}
};
static RendererData* renderer_data;
static RendererAPI* renderer_api = nullptr;
static RenderCommandQueue resource_free_queue[3];
static RenderCommandQueue* command_queue = nullptr;

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
	renderer_data = new RendererData();
	CORE_DEBUG("Initializing renderer.");
}

void Renderer::shut_down() { delete renderer_data; }

void Renderer::wait_and_render() { }

void Renderer::begin_frame() { }

void Renderer::end_frame() { }

RenderCommandQueue& Renderer::get_render_resource_free_queue(uint32_t index) { return resource_free_queue[index]; }

} // namespace ForgottenEngine
