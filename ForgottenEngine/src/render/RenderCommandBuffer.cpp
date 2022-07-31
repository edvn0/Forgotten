//
//  RenderCommandBuffer.cpp
//  Forgotten
//
//  Created by Edwin Carlsson on 2022-07-25.
//

#include "render/RenderCommandBuffer.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanRenderCommandBuffer.hpp"

namespace ForgottenEngine {

Reference<RenderCommandBuffer> RenderCommandBuffer::create(uint32_t count)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanRenderCommandBuffer>::create(count);
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

Reference<RenderCommandBuffer> RenderCommandBuffer::create_from_swapchain()
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanRenderCommandBuffer>::create();
	}
	CORE_ASSERT(false, "Unknown RendererAPI");
}

}
