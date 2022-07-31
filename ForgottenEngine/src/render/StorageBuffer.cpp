#include "fg_pch.hpp"

#include "render/StorageBuffer.hpp"

#include "render/RendererAPI.hpp"
#include "vulkan/VulkanStorageBuffer.hpp"

namespace ForgottenEngine {

Reference<StorageBuffer> StorageBuffer::create(uint32_t size, uint32_t binding)
{
	switch (RendererAPI::current()) {
	case RendererAPIType::None:
		return nullptr;
	case RendererAPIType::Vulkan:
		return Reference<VulkanStorageBuffer>::create(size, binding);
	}
	CORE_ASSERT(false, "Unknown RendererAPI!");
	return nullptr;
}

}
