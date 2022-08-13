//
// Created by Edwin Carlsson on 2022-07-06.
//

#pragma once

#include "Common.hpp"
#include "VulkanAllocatedBuffer.hpp"
#include "vk_mem_alloc.h"

namespace ForgottenEngine {

	class VulkanBuffer {
	private:
		VmaAllocator allocator;
		AllocatedBuffer allocated_buffer { nullptr, nullptr };

	public:
		explicit VulkanBuffer(VmaAllocator& allocator)
			: allocator(allocator) {};

		~VulkanBuffer() = default;

		void upload(size_t allocation_size, VkBufferUsageFlags usage, VmaMemoryUsage memory_usage);

		void destroy();

		void set_data(void* data, size_t size);

		AllocatedBuffer& get_buffer();
	};

	struct StagingInfo {
		VkBufferCreateInfo create_info;
		VmaAllocationCreateInfo allocation_info;
	};

	static constexpr StagingInfo DefaultStagingInfo {
		.create_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = nullptr,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		},
		.allocation_info = { .usage = VMA_MEMORY_USAGE_CPU_ONLY }
	};

	class VulkanStagingBuffer : public VulkanBuffer {
	private:
		StagingInfo info;

	public:
		explicit VulkanStagingBuffer(VmaAllocator& allocator, size_t size = 0, StagingInfo info = DefaultStagingInfo)
			: VulkanBuffer(allocator)
			, info(info)
		{
			if (size) {
				info.create_info.size = size;
			}
			auto& b = get_buffer();
			VK_CHECK(vmaCreateBuffer(allocator, &info.create_info, &info.allocation_info, &b.buffer, &b.allocation, nullptr));
		};
	};

} // namespace ForgottenEngine