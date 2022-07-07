//
// Created by Edwin Carlsson on 2022-07-06.
//

#pragma once

#include "VulkanAllocatedBuffer.hpp"

#include "vk_mem_alloc.h"

namespace ForgottenEngine {

class VulkanBuffer {
private:
	VmaAllocator allocator;
	AllocatedBuffer allocated_buffer{ nullptr, nullptr };

public:
	explicit VulkanBuffer(VmaAllocator& allocator)
		: allocator(allocator){};

	void upload(size_t allocation_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);
	void destroy();

	AllocatedBuffer& get_buffer();
};

}