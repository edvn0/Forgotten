//
// Created by Edwin Carlsson on 2022-07-06.
//

#include "fg_pch.hpp"

#include "Common.hpp"
#include "vulkan/VulkanBuffer.hpp"

namespace ForgottenEngine {

void VulkanBuffer::upload(size_t allocation_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage)
{
	// allocate vertex buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;

	bufferInfo.size = allocation_size;
	bufferInfo.usage = usage;

	VmaAllocationCreateInfo vai = {};
	vai.usage = memory_usage;

	// allocate the buffer
	VK_CHECK(vmaCreateBuffer(
		allocator, &bufferInfo, &vai, &allocated_buffer.buffer, &allocated_buffer.allocation, nullptr));
}

void VulkanBuffer::destroy() { vmaDestroyBuffer(allocator, allocated_buffer.buffer, allocated_buffer.allocation); }

AllocatedBuffer& VulkanBuffer::get_buffer() { return allocated_buffer; }

}