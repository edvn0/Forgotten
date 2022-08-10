#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/VulkanDevice.hpp"

#include <string>

namespace ForgottenEngine {

	struct GPUMemoryStats {
		uint64_t used = 0;
		uint64_t free = 0;
	};

	class VulkanAllocator {
	public:
		VulkanAllocator() = default;
		VulkanAllocator(const std::string& tag);
		~VulkanAllocator();

		VmaAllocation allocate_buffer(VkBufferCreateInfo bci, VmaMemoryUsage usage, VkBuffer& out_buffer);
		VmaAllocation allocate_image(VkImageCreateInfo ici, VmaMemoryUsage usage, VkImage& out_image);
		void free(VmaAllocation allocation);
		void destroy_image(VkImage image, VmaAllocation allocation);
		void destroy_buffer(VkBuffer buffer, VmaAllocation allocation);

		template <typename T>
		T* map_memory(VmaAllocation allocation)
		{
			T* mappedMemory;
			vmaMapMemory(VulkanAllocator::get_vma_allocator(), allocation, (void**)&mappedMemory);
			return mappedMemory;
		}

		void unmap_memory(VmaAllocation allocation);

		static void init(Reference<VulkanDevice> device);
		static void shutdown();

		static VmaAllocator& get_vma_allocator();

	private:
		std::string tag;
	};

} // namespace ForgottenEngine