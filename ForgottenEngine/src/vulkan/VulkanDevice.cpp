#include "vulkan/VulkanDevice.hpp"
#include "fg_pch.hpp"

#include "vulkan/VulkanContext.hpp"

#include <GLFW/glfw3.h>

#include "vulkan/VulkanInitializers.hpp"

#include "Application.hpp"
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

namespace ForgottenEngine {

VulkanPhysicalDevice::VulkanPhysicalDevice(vkb::Instance& vkb_inst)
{
	auto vkInstance = VulkanContext::get_instance();

	uint32_t gpuCount = 0;
	// Get number of available physical devices
	vkEnumeratePhysicalDevices(vkInstance, &gpuCount, nullptr);
	CORE_ASSERT(gpuCount > 0, "", "");
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	VK_CHECK(vkEnumeratePhysicalDevices(vkInstance, &gpuCount, physicalDevices.data()));

	VkPhysicalDevice selectedPhysicalDevice = nullptr;
	for (VkPhysicalDevice physicalDevice : physicalDevices) {
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
			selectedPhysicalDevice = physicalDevice;
			break;
		}
	}

	if (!selectedPhysicalDevice) {
		selectedPhysicalDevice = physicalDevices.back();
	}

	CORE_ASSERT(selectedPhysicalDevice, "Could not find any physical devices!", "");
	physical_device = selectedPhysicalDevice;

	vkGetPhysicalDeviceFeatures(physical_device, &features);
	vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
	CORE_ASSERT(queueFamilyCount > 0, "", "");
	queue_family_properties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queue_family_properties.data());

	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extCount, nullptr);
	if (extCount > 0) {
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extCount, &extensions.front())
			== VK_SUCCESS) {
			for (const auto& ext : extensions) {
				supported_extensions.emplace(ext.extensionName);
			}
		}
	}

	// Queue families
	// Desired queues need to be requested upon logical device creation
	// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially
	// if the application requests different queue types

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	static const float defaultQueuePriority(0.0f);

	int requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
	queue_family_indices = get_queue_family_indices(requestedQueueTypes);

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queue_family_indices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queue_create_infos.push_back(queueInfo);
	}

	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
		if (queue_family_indices.compute != queue_family_indices.graphics) {
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queue_family_indices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queue_create_infos.push_back(queueInfo);
		}
	}

	// Dedicated transfer queue
	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) {
		if ((queue_family_indices.transfer != queue_family_indices.graphics)
			&& (queue_family_indices.transfer != queue_family_indices.compute)) {
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queue_family_indices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queue_create_infos.push_back(queueInfo);
		}
	}

	depth_format = find_depth_format();
	CORE_ASSERT(depth_format, "");
}

VulkanPhysicalDevice::~VulkanPhysicalDevice() { }

VkFormat VulkanPhysicalDevice::find_depth_format() const
{
	// Since all depth formats may be optional, we need to find a suitable depth format to use
	// Start with the highest precision packed format
	std::vector<VkFormat> depthFormats = { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };

	// TODO: Move to VulkanPhysicalDevice
	for (auto& format : depthFormats) {
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &formatProps);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
			return format;
	}
	return VK_FORMAT_UNDEFINED;
}

bool VulkanPhysicalDevice::is_extension_supported(const std::string& extensionName) const
{
	return supported_extensions.find(extensionName) != supported_extensions.end();
}

VulkanPhysicalDevice::QueueFamilyIndices VulkanPhysicalDevice::get_queue_family_indices(int flags)
{
	QueueFamilyIndices indices;

	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (flags & VK_QUEUE_COMPUTE_BIT) {
		for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
			auto& queueFamilyProperties = queue_family_properties[i];
			if ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT)
				&& ((queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
				indices.compute = i;
				break;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (flags & VK_QUEUE_TRANSFER_BIT) {
		for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
			auto& queueFamilyProperties = queue_family_properties[i];
			if ((queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)
				&& ((queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
				&& ((queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
				indices.transfer = i;
				break;
			}
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the
	// requested flags
	for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
		if ((flags & VK_QUEUE_TRANSFER_BIT) && indices.transfer == -1) {
			if (queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
				indices.transfer = i;
		}

		if ((flags & VK_QUEUE_COMPUTE_BIT) && indices.compute == -1) {
			if (queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
				indices.compute = i;
		}

		if (flags & VK_QUEUE_GRAPHICS_BIT) {
			if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphics = i;
		}
	}

	return indices;
}

uint32_t VulkanPhysicalDevice::get_memory_type_index(uint32_t typeBits, VkMemoryPropertyFlags properties) const
{
	// Iterate over all memory types available for the device used in this example
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
		if ((typeBits & 1) == 1) {
			if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}
		typeBits >>= 1;
	}

	CORE_ASSERT(false, "Could not find a suitable memory type!", "");
	return UINT32_MAX;
}

Reference<VulkanPhysicalDevice> VulkanPhysicalDevice::select(vkb::Instance& instance)
{
	return Reference<VulkanPhysicalDevice>::create(instance);
}

VulkanDevice::VulkanDevice(const Reference<VulkanPhysicalDevice>& p_device, VkPhysicalDeviceFeatures enabled)
	: physical_device(p_device)
	, enabled_features(enabled)
{
	std::vector<const char*> device_exts;

	CORE_ASSERT(physical_device->is_extension_supported(VK_KHR_SWAPCHAIN_EXTENSION_NAME), "");
	device_exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	device_exts.push_back("VK_KHR_portability_subset");

	VkDeviceCreateInfo dci = {};
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = static_cast<uint32_t>(physical_device->queue_create_infos.size());
	dci.pQueueCreateInfos = physical_device->queue_create_infos.data();
	dci.pEnabledFeatures = &enabled_features;

	// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
	if (physical_device->is_extension_supported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
		device_exts.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
		enable_debug_markers = true;
	}

	if (!device_exts.empty()) {
		dci.enabledExtensionCount = (uint32_t)device_exts.size();
		dci.ppEnabledExtensionNames = device_exts.data();
	}

	VK_CHECK(vkCreateDevice(physical_device->get_vulkan_physical_device(), &dci, nullptr, &logical_device));

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = physical_device->queue_family_indices.graphics;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK(vkCreateCommandPool(logical_device, &cmdPoolInfo, nullptr, &command_pool));

	cmdPoolInfo.queueFamilyIndex = physical_device->queue_family_indices.compute;
	VK_CHECK(vkCreateCommandPool(logical_device, &cmdPoolInfo, nullptr, &compute_command_pool));

	// Get a graphics queue from the device
	vkGetDeviceQueue(logical_device, physical_device->queue_family_indices.graphics, 0, &graphics_queue);
	vkGetDeviceQueue(logical_device, physical_device->queue_family_indices.compute, 0, &compute_queue);
}

VulkanDevice::~VulkanDevice() = default;

void VulkanDevice::destroy()
{
	vkDestroyCommandPool(logical_device, command_pool, nullptr);
	vkDestroyCommandPool(logical_device, compute_command_pool, nullptr);

	vkDeviceWaitIdle(logical_device);
	vkDestroyDevice(logical_device, nullptr);
}

VkCommandBuffer VulkanDevice::get_command_buffer(bool begin, bool compute)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = compute ? compute_command_pool : command_pool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(logical_device, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin) {
		VkCommandBufferBeginInfo cmdBufferBeginInfo{};
		cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
	}

	return cmdBuffer;
}

void VulkanDevice::flush_command_buffer(VkCommandBuffer commandBuffer)
{
	flush_command_buffer(commandBuffer, graphics_queue);
}

void VulkanDevice::flush_command_buffer(VkCommandBuffer commandBuffer, VkQueue queue)
{
	const uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

	CORE_ASSERT(commandBuffer != VK_NULL_HANDLE, "");

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	VkFence fence;
	VK_CHECK(vkCreateFence(logical_device, &fenceCreateInfo, nullptr, &fence));

	// Submit to the queue
	VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
	// Wait for the fence to signal that command buffer has finished executing
	VK_CHECK(vkWaitForFences(logical_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));

	vkDestroyFence(logical_device, fence, nullptr);
	vkFreeCommandBuffers(logical_device, command_pool, 1, &commandBuffer);
}

VkCommandBuffer VulkanDevice::get_secondary_buffer() const
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = command_pool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	cmdBufAllocateInfo.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(logical_device, &cmdBufAllocateInfo, &cmdBuffer));
	return cmdBuffer;
}

}
