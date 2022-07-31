#include "render/VertexBuffer.hpp"

#include "vulkan/VulkanVertexBuffer.hpp"

namespace ForgottenEngine {

static Reference<VertexBuffer> create(void* data, uint32_t size, VertexBufferUsage usage)
{
	return Reference<VulkanVertexBuffer>::create(data, size, usage);
}
static Reference<VertexBuffer> create(uint32_t size, VertexBufferUsage usage)
{
	return Reference<VulkanVertexBuffer>::create(size, usage);
}

} // namespace ForgottenEngine
