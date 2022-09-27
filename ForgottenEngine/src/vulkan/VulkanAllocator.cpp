#include "fg_pch.hpp"

#include "vulkan/VulkanAllocator.hpp"

#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	struct VulkanAllocatorData {
		VmaAllocator allocator;
		uint64_t total_allocated_bytes = 0;
	};

	static VulkanAllocatorData& vma_data()
	{
		static VulkanAllocatorData* data_impl = nullptr;

		if (!data_impl) {
			data_impl = new VulkanAllocatorData();
			// Initialize VulkanMemoryAllocator
			auto device = VulkanContext::get_current_device();
			VmaAllocatorCreateInfo allocator_info = {};
			allocator_info.vulkanApiVersion = VK_API_VERSION_1_1;
			allocator_info.physicalDevice = device->get_physical_device()->get_vulkan_physical_device();
			allocator_info.device = device->get_vulkan_device();
			allocator_info.instance = VulkanContext::get_instance();

			vmaCreateAllocator(&allocator_info, &data_impl->allocator);
		}

		return *data_impl;
	}

	VulkanAllocator::VulkanAllocator(const std::string& tag)
		: tag(tag)
	{
	}

	VulkanAllocator::~VulkanAllocator() { }

	VmaAllocation VulkanAllocator::allocate_buffer(VkBufferCreateInfo bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		vmaCreateBuffer(vma_data().allocator, &bufferCreateInfo, &allocCreateInfo, &outBuffer, &allocation, nullptr);

		// TODO: Tracking
		VmaAllocationInfo allocInfo {};
		vmaGetAllocationInfo(vma_data().allocator, allocation, &allocInfo);
		{
			vma_data().total_allocated_bytes += allocInfo.size;
		}

		return allocation;
	}

	VmaAllocation VulkanAllocator::allocate_image(VkImageCreateInfo imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		vmaCreateImage(vma_data().allocator, &imageCreateInfo, &allocCreateInfo, &outImage, &allocation, nullptr);

		// TODO: Tracking
		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(vma_data().allocator, allocation, &allocInfo);
		{
			vma_data().total_allocated_bytes += allocInfo.size;
		}
		return allocation;
	}

	void VulkanAllocator::free(VmaAllocation allocation) { vmaFreeMemory(vma_data().allocator, allocation); }

	void VulkanAllocator::destroy_image(VkImage image, VmaAllocation allocation)
	{
		core_assert_bool(image);
		core_assert_bool(allocation);
		vmaDestroyImage(vma_data().allocator, image, allocation);
	}

	void VulkanAllocator::destroy_buffer(VkBuffer buffer, VmaAllocation allocation)
	{
		core_assert_bool(buffer);
		core_assert_bool(allocation);
		vmaDestroyBuffer(vma_data().allocator, buffer, allocation);
	}

	void VulkanAllocator::unmap_memory(VmaAllocation allocation) { vmaUnmapMemory(vma_data().allocator, allocation); }

	void VulkanAllocator::shutdown() { vmaDestroyAllocator(vma_data().allocator); }

	VmaAllocator& VulkanAllocator::get_vma_allocator() { return vma_data().allocator; }

} // namespace ForgottenEngine
