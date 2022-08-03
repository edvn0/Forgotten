#include "fg_pch.hpp"

#include "render/Renderer.hpp"
#include "render/RendererAPI.hpp"

#include "render/IndexBuffer.hpp"
#include "vulkan/VulkanIndexBuffer.hpp"

namespace ForgottenEngine {

Reference<IndexBuffer> IndexBuffer::create(void* data, uint32_t size)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanIndexBuffer>::create(data, size);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

Reference<IndexBuffer> IndexBuffer::create(uint32_t size)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanIndexBuffer>::create(size);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

} // namespace ForgottenEngine
