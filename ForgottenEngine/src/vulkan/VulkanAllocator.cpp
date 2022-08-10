#include "fg_pch.hpp"

#include "vulkan/VulkanAllocator.hpp"

#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

	struct VulkanAllocatorData {
		VmaAllocator allocator;
		uint64_t total_allocated_bytes = 0;
	};

	static VulkanAllocatorData* vma_data = nullptr;

	VulkanAllocator::VulkanAllocator(const std::string& tag)
		: tag(tag)
	{
	}

	VulkanAllocator::~VulkanAllocator() { }

	VmaAllocation VulkanAllocator::allocate_buffer(
		VkBufferCreateInfo bufferCreateInfo, VmaMemoryUsage usage, VkBuffer& outBuffer)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		vmaCreateBuffer(vma_data->allocator, &bufferCreateInfo, &allocCreateInfo, &outBuffer, &allocation, nullptr);

		// TODO: Tracking
		VmaAllocationInfo allocInfo {};
		vmaGetAllocationInfo(vma_data->allocator, allocation, &allocInfo);
		{
			vma_data->total_allocated_bytes += allocInfo.size;
		}

		return allocation;
	}

	VmaAllocation VulkanAllocator::allocate_image(
		VkImageCreateInfo imageCreateInfo, VmaMemoryUsage usage, VkImage& outImage)
	{
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = usage;

		VmaAllocation allocation;
		vmaCreateImage(vma_data->allocator, &imageCreateInfo, &allocCreateInfo, &outImage, &allocation, nullptr);

		// TODO: Tracking
		VmaAllocationInfo allocInfo;
		vmaGetAllocationInfo(vma_data->allocator, allocation, &allocInfo);
		{
			vma_data->total_allocated_bytes += allocInfo.size;
		}
		return allocation;
	}

	void VulkanAllocator::free(VmaAllocation allocation) { vmaFreeMemory(vma_data->allocator, allocation); }

	void VulkanAllocator::destroy_image(VkImage image, VmaAllocation allocation)
	{
		CORE_ASSERT(image, "");
		CORE_ASSERT(allocation, "");
		vmaDestroyImage(vma_data->allocator, image, allocation);
	}

	void VulkanAllocator::destroy_buffer(VkBuffer buffer, VmaAllocation allocation)
	{
		CORE_ASSERT(buffer, "");
		CORE_ASSERT(allocation, "");
		vmaDestroyBuffer(vma_data->allocator, buffer, allocation);
	}

	void VulkanAllocator::unmap_memory(VmaAllocation allocation) { vmaUnmapMemory(vma_data->allocator, allocation); }

	void VulkanAllocator::init(Reference<VulkanDevice> device)
	{
		vma_data = new VulkanAllocatorData();

		// Initialize VulkanMemoryAllocator
		VmaAllocatorCreateInfo allocatorInfo = {};
		allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
		allocatorInfo.physicalDevice = device->get_physical_device()->get_vulkan_physical_device();
		allocatorInfo.device = device->get_vulkan_device();
		allocatorInfo.instance = VulkanContext::get_instance();

		vmaCreateAllocator(&allocatorInfo, &vma_data->allocator);
	}

	void VulkanAllocator::shutdown()
	{
		vmaDestroyAllocator(vma_data->allocator);

		delete vma_data;
		vma_data = nullptr;
	}

	VmaAllocator& VulkanAllocator::get_vma_allocator() { return vma_data->allocator; }

} // namespace ForgottenEngine
