#pragma once

#include "Reference.hpp"

#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

#include <unordered_set>
#include <vector>

namespace vkb {
class Instance;
}

namespace ForgottenEngine {

class VulkanPhysicalDevice : public ReferenceCounted {
public:
	struct QueueFamilyIndices {
		int32_t graphics = -1;
		int32_t compute = -1;
		int32_t transfer = -1;
	};

public:
	VulkanPhysicalDevice(vkb::Instance& instance);
	~VulkanPhysicalDevice();

	bool is_extension_supported(const std::string& extension) const;
	uint32_t get_memory_type_index(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

	VkPhysicalDevice get_vulkan_physical_device() const { return physical_device; }
	const QueueFamilyIndices& get_queue_family_indices() const { return queue_family_indices; }

	const VkPhysicalDeviceProperties& get_properties() const { return properties; }
	const VkPhysicalDeviceLimits& get_limits() const { return properties.limits; }
	const VkPhysicalDeviceMemoryProperties& get_memory_properties() const { return memory_properties; }

	VkFormat get_depth_format() const { return depth_format; }

	static Reference<VulkanPhysicalDevice> select(vkb::Instance& instance);

private:
	VkFormat find_depth_format() const;
	QueueFamilyIndices get_queue_family_indices(int queueFlags);

private:
	QueueFamilyIndices queue_family_indices;

	VkPhysicalDevice physical_device = nullptr;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memory_properties;

	VkFormat depth_format = VK_FORMAT_UNDEFINED;

	std::vector<VkQueueFamilyProperties> queue_family_properties;
	std::unordered_set<std::string> supported_extensions;
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

	friend class VulkanDevice;
};

// Represents a logical device
class VulkanDevice : public ReferenceCounted {
public:
	VulkanDevice(
		const Reference<VulkanPhysicalDevice>& physical_device, VkPhysicalDeviceFeatures enabled_features);
	~VulkanDevice();

	void destroy();

	VkQueue get_graphics_queue() { return graphics_queue; }
	VkQueue get_compute_queue() { return compute_queue; }

	VkCommandBuffer get_command_buffer(bool begin, bool compute = false);
	void flush_command_buffer(VkCommandBuffer command_buffer);
	void flush_command_buffer(VkCommandBuffer command_buffer, VkQueue queue);

	VkCommandBuffer get_secondary_buffer() const;

	const Reference<VulkanPhysicalDevice>& get_physical_device() const { return physical_device; }
	VkDevice get_vulkan_device() const { return logical_device; }

private:
	VkDevice logical_device = nullptr;
	Reference<VulkanPhysicalDevice> physical_device;
	VkPhysicalDeviceFeatures enabled_features;
	VkCommandPool command_pool = nullptr, compute_command_pool = nullptr;

	VkQueue graphics_queue;
	VkQueue compute_queue;

	bool enable_debug_markers = false;
};

}
