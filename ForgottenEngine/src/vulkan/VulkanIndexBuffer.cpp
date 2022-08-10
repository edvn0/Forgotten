#include "fg_pch.hpp"

#include "vulkan/VulkanIndexBuffer.hpp"

#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	VulkanIndexBuffer::VulkanIndexBuffer(uint32_t size)
		: size(size)
	{
	}

	VulkanIndexBuffer::VulkanIndexBuffer(void* data, uint32_t size)
		: size(size)
	{
		local_data = Buffer::copy(data, size);

		Reference<VulkanIndexBuffer> instance = this;
		Renderer::submit([instance]() mutable {
			auto device = VulkanContext::get_current_device();
			VulkanAllocator allocator("IndexBuffer");

			VkBufferCreateInfo bci {};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.size = instance->size;
			bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer stagingBuffer;
			VmaAllocation stagingBufferAllocation
				= allocator.allocate_buffer(bci, VMA_MEMORY_USAGE_CPU_TO_GPU, stagingBuffer);

			// Copy data to staging buffer
			uint8_t* destData = allocator.map_memory<uint8_t>(stagingBufferAllocation);
			memcpy(destData, instance->local_data.data, instance->local_data.size);
			allocator.unmap_memory(stagingBufferAllocation);

			VkBufferCreateInfo indexBufferCreateInfo = {};
			indexBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			indexBufferCreateInfo.size = instance->size;
			indexBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			instance->memory_allocation
				= allocator.allocate_buffer(indexBufferCreateInfo, VMA_MEMORY_USAGE_GPU_ONLY, instance->vulkan_buffer);

			VkCommandBuffer copyCmd = device->get_command_buffer(true);

			VkBufferCopy copyRegion = {};
			copyRegion.size = instance->local_data.size;
			vkCmdCopyBuffer(copyCmd, stagingBuffer, instance->vulkan_buffer, 1, &copyRegion);

			device->flush_command_buffer(copyCmd);

			allocator.destroy_buffer(stagingBuffer, stagingBufferAllocation);
		});
	}

	VulkanIndexBuffer::~VulkanIndexBuffer()
	{
		VkBuffer buffer = vulkan_buffer;
		VmaAllocation allocation = memory_allocation;
		Renderer::submit_resource_free([buffer, allocation]() {
			VulkanAllocator allocator("IndexBuffer");
			allocator.destroy_buffer(buffer, allocation);
		});
		local_data.release();
	}

	void VulkanIndexBuffer::set_data(void* buffer, uint32_t size, uint32_t offset) { }

	void VulkanIndexBuffer::bind() const { }

	RendererID VulkanIndexBuffer::get_renderer_id() const { return 0; }

} // namespace ForgottenEngine
