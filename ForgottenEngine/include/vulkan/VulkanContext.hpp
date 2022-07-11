#pragma once

#include <functional>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace ForgottenEngine {

class VulkanContext {
private:
	VulkanContext() = default;

public:
	static VulkanContext& the()
	{
		static VulkanContext instance; // Guaranteed to be destroyed.
		return instance;
	}

public:
	VulkanContext(const VulkanContext& vc) = delete;
	void operator=(const VulkanContext& vc) = delete;

public:
	static void construct_and_initialize(GLFWwindow* handle);

	static VkInstance get_instance();
	static VkSurfaceKHR get_surface();
	static VkAllocationCallbacks* get_allocator();
	static uint32_t get_queue_family();
	static VkQueue get_queue();
	static VkPhysicalDevice get_physical_device();
	static VkDevice get_device();
	static uint32_t get_alignment();
	static GLFWwindow* get_window_handle();
	static std::pair<int, int> get_framebuffer_size();
	static float get_dpi();
	static void cleanup();
};

}