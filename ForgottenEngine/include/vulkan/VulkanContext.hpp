#pragma once

#include <functional>
#include <vulkan/vulkan.h>

namespace ForgottenEngine {

class VulkanContext {
private:
	VulkanContext() = default;

	bool initialized{ false };

	static void construct_and_initialize();

public:
	static VulkanContext& the()
	{
		static VulkanContext instance; // Guaranteed to be destroyed.

		if (!instance.initialized) {
			VulkanContext::construct_and_initialize();
			instance.initialized = true;
		}

		return instance;
	}

public:
	VulkanContext(const VulkanContext& vc) = delete;
	void operator=(const VulkanContext& vc) = delete;

public:
	static VkInstance get_instance();
	static VkAllocationCallbacks* get_allocator();
	static uint32_t get_queue_family();
	static VkQueue get_queue();
	static VkPhysicalDevice get_physical_device();
	static VkDevice get_device();
	static void cleanup();
};

}