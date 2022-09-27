#include "fg_pch.hpp"

#include "vulkan/VulkanDevice.hpp"

#include "vk_mem_alloc.h"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanInitializers.hpp"

namespace ForgottenEngine {

	VulkanPhysicalDevice::VulkanPhysicalDevice()
	{
		auto vk_instance = VulkanContext::get_instance();

		uint32_t gpu_count = 0;
		// Get number of available physical devices
		vkEnumeratePhysicalDevices(vk_instance, &gpu_count, nullptr);
		core_assert_bool(gpu_count > 0);
		// Enumerate devices
		std::vector<VkPhysicalDevice> devices_to_select_from(gpu_count);
		vk_check(vkEnumeratePhysicalDevices(vk_instance, &gpu_count, devices_to_select_from.data()));

		VkPhysicalDevice selected_physical_device;
		for (auto physical : devices_to_select_from) {

			vkGetPhysicalDeviceProperties(physical, &properties);
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				selected_physical_device = physical;
				break;
			}

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
				selected_physical_device = physical;
				break;
			}
		}

		if (!selected_physical_device) {
			selected_physical_device = devices_to_select_from.back();
		}

		core_assert(selected_physical_device, "Could not find any physical devices!");
		physical_device = std::move(selected_physical_device);

		vkGetPhysicalDeviceFeatures(physical_device, &features);
		vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

		uint32_t family_count;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, nullptr);
		core_assert_bool(family_count > 0);
		queue_family_properties.resize(family_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &family_count, queue_family_properties.data());

		uint32_t extension_count = 0;
		vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
		if (extension_count > 0) {
			std::vector<VkExtensionProperties> extensions(extension_count);

			vk_check(vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, &extensions.front()));

			for (const auto& ext : extensions) {
				supported_extensions.emplace(ext.extensionName);
			}
		}

		static const float default_queue_priority(0.0f);

		int requested_queue_types = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
		queue_family_indices = get_queue_family_indices(requested_queue_types);

		// Graphics queue
		if (requested_queue_types & VK_QUEUE_GRAPHICS_BIT) {
			VkDeviceQueueCreateInfo queue_info {};
			queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_info.queueFamilyIndex = queue_family_indices.graphics;
			queue_info.queueCount = 1;
			queue_info.pQueuePriorities = &default_queue_priority;
			queue_create_infos.push_back(queue_info);
		}

		// Dedicated compute queue
		if (requested_queue_types & VK_QUEUE_COMPUTE_BIT) {
			if (queue_family_indices.compute != queue_family_indices.graphics) {
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queue_info {};
				queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_info.queueFamilyIndex = queue_family_indices.compute;
				queue_info.queueCount = 1;
				queue_info.pQueuePriorities = &default_queue_priority;
				queue_create_infos.push_back(queue_info);
			}
		}

		// Dedicated transfer queue
		if (requested_queue_types & VK_QUEUE_TRANSFER_BIT) {
			if ((queue_family_indices.transfer != queue_family_indices.graphics) && (queue_family_indices.transfer != queue_family_indices.compute)) {
				// If compute family index differs, we need an additional queue create info for the compute queue
				VkDeviceQueueCreateInfo queue_info {};
				queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_info.queueFamilyIndex = queue_family_indices.transfer;
				queue_info.queueCount = 1;
				queue_info.pQueuePriorities = &default_queue_priority;
				queue_create_infos.push_back(queue_info);
			}
		}

		depth_format = find_depth_format();
		core_assert_bool(depth_format);
	}

	VulkanPhysicalDevice::~VulkanPhysicalDevice() = default;

	VkFormat VulkanPhysicalDevice::find_depth_format() const
	{
		// Since all depth formats may be optional, we need to find a suitable depth format to use
		// Start with the highest precision packed format
		std::vector<VkFormat> depth_formats
			= { VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };

		for (auto& format : depth_formats) {
			VkFormatProperties format_props;
			vkGetPhysicalDeviceFormatProperties(physical_device, format, &format_props);
			// Format must support depth stencil attachment for optimal tiling
			if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
				return format;
		}
		return VK_FORMAT_UNDEFINED;
	}

	bool VulkanPhysicalDevice::is_extension_supported(const std::string& extension_name) const
	{
		return supported_extensions.find(extension_name) != supported_extensions.end();
	}

	VulkanPhysicalDevice::QueueFamilyIndices VulkanPhysicalDevice::get_queue_family_indices(int flags)
	{
		QueueFamilyIndices indices;

		// Dedicated queue for compute
		// Try to find a queue family index that supports compute but not graphics
		if (flags & VK_QUEUE_COMPUTE_BIT) {
			for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
				const auto& props = queue_family_properties[i];
				if ((props.queueFlags & VK_QUEUE_COMPUTE_BIT) && ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
					indices.compute = static_cast<int32_t>(i);
					break;
				}
			}
		}

		// Dedicated queue for transfer
		// Try to find a queue family index that supports transfer but not graphics and compute
		if (flags & VK_QUEUE_TRANSFER_BIT) {
			for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
				const auto& props = queue_family_properties[i];
				if ((props.queueFlags & VK_QUEUE_TRANSFER_BIT) && ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
					&& ((props.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
					indices.transfer = static_cast<int32_t>(i);
					break;
				}
			}
		}

		// For other queue types or if no separate compute queue is present, return the first one to support the
		// requested flags
		for (uint32_t i = 0; i < queue_family_properties.size(); i++) {
			if ((flags & VK_QUEUE_TRANSFER_BIT) && indices.transfer == -1) {
				if (queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
					indices.transfer = static_cast<int32_t>(i);
			}

			if ((flags & VK_QUEUE_COMPUTE_BIT) && indices.compute == -1) {
				if (queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
					indices.compute = static_cast<int32_t>(i);
			}

			if (flags & VK_QUEUE_GRAPHICS_BIT) {
				if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
					indices.graphics = static_cast<int32_t>(i);
			}
		}

		return indices;
	}

	Reference<VulkanPhysicalDevice> VulkanPhysicalDevice::select() { return Reference<VulkanPhysicalDevice>::create(); }

	VulkanDevice::VulkanDevice(const Reference<VulkanPhysicalDevice>& p_device, VkPhysicalDeviceFeatures enabled)
		: physical_device(p_device)
		, enabled_features(enabled)
	{
		std::vector<const char*> device_exts;

		core_assert_bool(physical_device->is_extension_supported(VK_KHR_SWAPCHAIN_EXTENSION_NAME));
		device_exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

#ifdef FORGOTTEN_MACOS
		device_exts.push_back("VK_KHR_portability_subset");
#endif

		VkDeviceCreateInfo dci = {};
		dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		dci.queueCreateInfoCount = static_cast<uint32_t>(physical_device->queue_create_infos.size());
		dci.pQueueCreateInfos = physical_device->queue_create_infos.data();
		dci.pEnabledFeatures = &enabled_features;

		// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
		if (physical_device->is_extension_supported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
			device_exts.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
		}

		if (!device_exts.empty()) {
			dci.enabledExtensionCount = (uint32_t)device_exts.size();
			dci.ppEnabledExtensionNames = device_exts.data();
		}

		vk_check(vkCreateDevice(physical_device->get_vulkan_physical_device(), &dci, nullptr, &logical_device));

		VkCommandPoolCreateInfo cmd_pool_info = {};
		cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmd_pool_info.queueFamilyIndex = physical_device->queue_family_indices.graphics;
		cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		vk_check(vkCreateCommandPool(logical_device, &cmd_pool_info, nullptr, &command_pool));

		cmd_pool_info.queueFamilyIndex = physical_device->queue_family_indices.compute;
		vk_check(vkCreateCommandPool(logical_device, &cmd_pool_info, nullptr, &compute_command_pool));

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
		VkCommandBuffer cmd_buffer;

		VkCommandBufferAllocateInfo cbai = {};
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.commandPool = compute ? compute_command_pool : command_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbai.commandBufferCount = 1;

		vk_check(vkAllocateCommandBuffers(logical_device, &cbai, &cmd_buffer));

		if (begin) {
			VkCommandBufferBeginInfo cmd_buffer_begin_info {};
			cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			vk_check(vkBeginCommandBuffer(cmd_buffer, &cmd_buffer_begin_info));
		}

		return cmd_buffer;
	}

	void VulkanDevice::flush_command_buffer(VkCommandBuffer command_buffer) { flush_command_buffer(command_buffer, graphics_queue); }

	void VulkanDevice::flush_command_buffer(VkCommandBuffer command_buffer, VkQueue queue)
	{
		static constexpr uint64_t default_fence_timeout = 100000000000;

		core_assert_bool(command_buffer != VK_NULL_HANDLE);

		vk_check(vkEndCommandBuffer(command_buffer));

		VkSubmitInfo qsi = {};
		qsi.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		qsi.commandBufferCount = 1;
		qsi.pCommandBuffers = &command_buffer;

		// Create fence to ensure that the command buffer has finished executing
		VkFenceCreateInfo fci = {};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fci.flags = 0;
		VkFence fence;
		vk_check(vkCreateFence(logical_device, &fci, nullptr, &fence));

		// Submit to the queue
		vk_check(vkQueueSubmit(queue, 1, &qsi, fence));
		// Wait for the fence to signal that command buffer has finished executing
		vk_check(vkWaitForFences(logical_device, 1, &fence, VK_TRUE, default_fence_timeout));

		vkDestroyFence(logical_device, fence, nullptr);
		vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
	}

	VkCommandBuffer VulkanDevice::get_secondary_buffer() const
	{
		VkCommandBuffer cmd_buffer;

		VkCommandBufferAllocateInfo cbai = {};
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.commandPool = command_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
		cbai.commandBufferCount = 1;

		vk_check(vkAllocateCommandBuffers(logical_device, &cbai, &cmd_buffer));
		return cmd_buffer;
	}

} // namespace ForgottenEngine
