#include "render/IndexBuffer.hpp"

#include "vulkan/VulkanIndexBuffer.hpp"

namespace ForgottenEngine {

Reference<IndexBuffer> create(uint32_t size) { return Reference<VulkanIndexBuffer>::create(size); }

Reference<IndexBuffer> create(void* data, uint32_t size)
{
	return Reference<VulkanIndexBuffer>::create(data, size);
}

} // namespace ForgottenEngine
