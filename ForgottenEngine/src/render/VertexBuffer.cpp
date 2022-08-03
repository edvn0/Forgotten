#include "fg_pch.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"

#include "render/VertexBuffer.hpp"
#include "vulkan/VulkanVertexBuffer.hpp"

namespace ForgottenEngine {

static Reference<VertexBuffer> create(void* data, uint32_t size, VertexBufferUsage usage)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanVertexBuffer>::create(data, size, usage);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}
static Reference<VertexBuffer> create(uint32_t size, VertexBufferUsage usage)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanVertexBuffer>::create(size, usage);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

} // namespace ForgottenEngine
