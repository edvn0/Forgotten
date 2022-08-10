#include "fg_pch.hpp"

#include "render/VertexBuffer.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"
#include "vulkan/VulkanVertexBuffer.hpp"

namespace ForgottenEngine {

	Reference<VertexBuffer> VertexBuffer::create(void* data, uint32_t size, VertexBufferUsage usage)
	{
		switch (RendererAPI::current()) {
		case RendererAPIType::None:
			return nullptr;
		case RendererAPIType::Vulkan:
			return Reference<VulkanVertexBuffer>::create(data, size, usage);
		}
		CORE_ASSERT(false, "Unknown RendererAPI");
	}
	Reference<VertexBuffer> VertexBuffer::create(uint32_t size, VertexBufferUsage usage)
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
