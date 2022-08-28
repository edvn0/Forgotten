//
// Created by Edwin Carlsson on 2022-08-05.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanUniformBuffer.hpp"

#include "render/Renderer.hpp"
#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	VulkanUniformBuffer::VulkanUniformBuffer(uint32_t in_size, uint32_t in_binding)
		: size(in_size)
		, binding(in_binding)
	{
		local_storage = hnew uint8_t[in_size];

		Reference<VulkanUniformBuffer> instance = this;
		Renderer::submit([instance]() mutable { instance->rt_invalidate(); });
	}

	VulkanUniformBuffer::~VulkanUniformBuffer() { release(); }

	void VulkanUniformBuffer::release()
	{
		if (!memory_alloc)
			return;

		Renderer::submit_resource_free([buffer = vk_buffer, memoryAlloc = memory_alloc]() {
			VulkanAllocator allocator("UniformBuffer");
			allocator.destroy_buffer(buffer, memoryAlloc);
		});

		vk_buffer = nullptr;
		memory_alloc = nullptr;

		delete[] local_storage;
		local_storage = nullptr;
	}

	void VulkanUniformBuffer::rt_invalidate()
	{
		release();

		VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.allocationSize = 0;
		allocInfo.memoryTypeIndex = 0;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		bufferInfo.size = size;

		VulkanAllocator allocator("UniformBuffer");
		memory_alloc = allocator.allocate_buffer(bufferInfo, VMA_MEMORY_USAGE_CPU_TO_GPU, vk_buffer);

		descriptor_buffer_info.buffer = vk_buffer;
		descriptor_buffer_info.offset = 0;
		descriptor_buffer_info.range = size;
	}

	void VulkanUniformBuffer::set_data(const void* data, uint32_t in_size, uint32_t offset)
	{
		// TODO: local storage should be potentially replaced with render thread storage
		memcpy(local_storage, data, in_size);
		Reference<VulkanUniformBuffer> instance = this;
		Renderer::submit([instance, in_size, offset]() mutable { instance->rt_set_data(instance->local_storage, in_size, offset); });
	}

	void VulkanUniformBuffer::rt_set_data(const void* data, uint32_t in_size, uint32_t offset)
	{
		VulkanAllocator allocator("VulkanUniformBuffer");
		uint8_t* pData = allocator.map_memory<uint8_t>(memory_alloc);
		memcpy(pData, (const uint8_t*)data + offset, in_size);
		allocator.unmap_memory(memory_alloc);
	}

} // namespace ForgottenEngine