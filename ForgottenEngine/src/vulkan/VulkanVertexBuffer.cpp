#include "fg_pch.hpp"

#include "vulkan/VulkanVertexBuffer.hpp"

#include "render/Renderer.hpp"
#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanContext.hpp"

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

			instance->memory_allocation = allocator.allocate_buffer(vbci, VMA_MEMORY_USAGE_CPU_TO_GPU, instance->vulkan_buffer);
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

			VkBufferCreateInfo bci {};
			bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bci.size = instance->size;
			bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VkBuffer staging_buffer;
			VmaAllocation staging_buffer_allocation = allocator.allocate_buffer(bci, VMA_MEMORY_USAGE_CPU_TO_GPU, staging_buffer);

			// copy data to staging buffer
			auto* dest_data = allocator.map_memory<uint8_t>(staging_buffer_allocation);
			memcpy(dest_data, instance->local_data.data, instance->local_data.size);
			allocator.unmap_memory(staging_buffer_allocation);

			VkBufferCreateInfo vbci = {};
			vbci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			vbci.size = instance->size;
			vbci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			instance->memory_allocation = allocator.allocate_buffer(vbci, VMA_MEMORY_USAGE_GPU_ONLY, instance->vulkan_buffer);

			VkCommandBuffer copy_cmd = device->get_command_buffer(true);

			VkBufferCopy copy_region = {};
			copy_region.size = instance->local_data.size;
			vkCmdCopyBuffer(copy_cmd, staging_buffer, instance->vulkan_buffer, 1, &copy_region);

			device->flush_command_buffer(copy_cmd);

			allocator.destroy_buffer(staging_buffer, staging_buffer_allocation);
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

	void VulkanVertexBuffer::set_data(void* buffer, uint32_t in_size, uint32_t offset)
	{
		core_assert(in_size <= local_data.size, "Size is less than local in_size.");
		memcpy(local_data.data, (uint8_t*)buffer + offset, in_size);
		;
		Reference<VulkanVertexBuffer> instance = this;
		Renderer::submit([instance, in_size, offset]() mutable { instance->rt_set_data(instance->local_data.data, in_size, offset); });
	}

	void VulkanVertexBuffer::rt_set_data(void* buffer, uint32_t in_size, uint32_t offset)
	{
		VulkanAllocator allocator("VulkanVertexBuffer");
		auto* data = allocator.map_memory<uint8_t>(memory_allocation);
		memcpy(data, (uint8_t*)buffer + offset, in_size);
		allocator.unmap_memory(memory_allocation);
	}

} // namespace ForgottenEngine
