#include "fg_pch.hpp"

#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanVertexBuffer.hpp"

#include "render/Renderer.hpp"

namespace ForgottenEngine {

VulkanVertexBuffer::VulkanVertexBuffer(uint32_t size, VertexBufferUsage usage)
	: size(size)
{
	local_data.allocate(size);

	Reference<VulkanVertexBuffer> instance = this;
	Renderer::submit([instance]() mutable {
		auto device = VulkanContext::get_current_device();
		VulkanAllocator allocator("VertexBuffer");

		VkBufferCreateInfo vbci = {};
		vbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vbci.size = instance->size;
		vbci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

		instance->memory_allocation
			= allocator.allocate_buffer(vbci, VMA_MEMORY_USAGE_CPU_TO_GPU, instance->vulkan_buffer);
	});
}

VulkanVertexBuffer::VulkanVertexBuffer(void* data, uint32_t size, VertexBufferUsage usage)
	: size(size)
{
	local_data = Buffer::copy(data, size);

	Reference<VulkanVertexBuffer> instance = this;
	Renderer::submit([instance]() mutable {
		auto device = VulkanContext::get_current_device();
		VulkanAllocator allocator("VertexBuffer");

		VkBufferCreateInfo bci{};
		bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bci.size = instance->size;
		bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkBuffer stagingBuffer;
		VmaAllocation stagingBufferAllocation
			= allocator.allocate_buffer(bci, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

		// copy data to staging buffer
		uint8_t* destData = allocator.map_memory<uint8_t>(stagingBufferAllocation);
		memcpy(destData, instance->local_data.data, instance->local_data.size);
		allocator.unmap_memory(stagingBufferAllocation);

		VkBufferCreateInfo vbci = {};
		vbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vbci.size = instance->size;
		vbci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		instance->memory_allocation
			= allocator.allocate_buffer(vbci, VMA_MEMORY_USAGE_GPU_ONLY, instance->vulkan_buffer);

		VkCommandBuffer copyCmd = device->get_command_buffer(true);

		VkBufferCopy copyRegion = {};
		copyRegion.size = instance->local_data.size;
		vkCmdCopyBuffer(copyCmd, stagingBuffer, instance->vulkan_buffer, 1, &copyRegion);

		device->flush_command_buffer(copyCmd);

		allocator.destroy_buffer(stagingBuffer, stagingBufferAllocation);
	});
}

VulkanVertexBuffer::~VulkanVertexBuffer()
{
	VkBuffer buffer = vulkan_buffer;
	VmaAllocation allocation = memory_allocation;
	Renderer::submit_resource_free([buffer, allocation]() {
		VulkanAllocator allocator("VertexBuffer");
		allocator.destroy_buffer(buffer, allocation);
	});

	local_data.release();
}

void VulkanVertexBuffer::set_data(void* buffer, uint32_t size, uint32_t offset)
{
	CORE_ASSERT(size <= local_data.size, "");
	memcpy(local_data.data, (uint8_t*)buffer + offset, size);
	;
	Reference<VulkanVertexBuffer> instance = this;
	Renderer::submit(
		[instance, size, offset]() mutable { instance->rt_set_data(instance->local_data.data, size, offset); });
}

void VulkanVertexBuffer::rt_set_data(void* buffer, uint32_t size, uint32_t offset)
{
	VulkanAllocator allocator("VulkanVertexBuffer");
	uint8_t* pData = allocator.map_memory<uint8_t>(memory_allocation);
	memcpy(pData, (uint8_t*)buffer + offset, size);
	allocator.unmap_memory(memory_allocation);
}

}
