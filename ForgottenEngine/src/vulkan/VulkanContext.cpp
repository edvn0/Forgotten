//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "vulkan/VulkanContext.hpp"
#include "GLFW/glfw3.h"
#include "fg_pch.hpp"

namespace ForgottenEngine {

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw error %d: %s\n", error, description);
}

#define IMGUI_VULKAN_DEBUG_REPORT

static VkAllocationCallbacks* global_allocator = NULL;
static VkInstance global_instance = VK_NULL_HANDLE;
static VkPhysicalDevice global_physical_device = VK_NULL_HANDLE;
static VkDevice global_device = VK_NULL_HANDLE;
static uint32_t global_queue_family = (uint32_t)-1;
static VkQueue global_queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT global_debug_report = VK_NULL_HANDLE;
static VkPipelineCache global_pipeline_cache = VK_NULL_HANDLE;
static VkDescriptorPool global_descriptor_pool = VK_NULL_HANDLE;

static int global_min_image_count = 2;

// Per-frame-in-flight
static std::vector<std::vector<VkCommandBuffer>> allocated_framebuffers;
static std::vector<std::vector<std::function<void()>>> resource_free_queue;

// Unlike global_main_window_data.FrameIndex, this is not the the swapchain image index
// and is always guaranteed to increase (eg. 0, 1, 2, 0, 1, 2)
static uint32_t s_CurrentFrameIndex = 0;

void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

#ifdef IMGUI_VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode,
	const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	(void)flags;
	(void)object;
	(void)location;
	(void)messageCode;
	(void)pUserData;
	(void)pLayerPrefix; // Unused arguments
	fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
	return VK_FALSE;
}
#endif // IMGUI_VULKAN_DEBUG_REPORT

static void setup_vulkan(const char** extensions, uint32_t extensions_count)
{
	VkResult err;

	// Create Vulkan Instance
	{
		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.enabledExtensionCount = extensions_count;
		create_info.ppEnabledExtensionNames = extensions;
#ifdef IMGUI_VULKAN_DEBUG_REPORT
		// Enabling validation layers
		const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = layers;

		// Enable debug report extension (we need additional storage, so we duplicate the user array to add our new
		// extension to it)
		const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions_count + 2));
		memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
		extensions_ext[extensions_count] = "VK_EXT_debug_report";
		extensions_ext[extensions_count + 1] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
		create_info.enabledExtensionCount = extensions_count + 2;
		create_info.ppEnabledExtensionNames = extensions_ext;
		create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

		// Create Vulkan Instance
		err = vkCreateInstance(&create_info, global_allocator, &global_instance);
		check_vk_result(err);
		free(extensions_ext);

		// Get the function pointer (required for any extensions)
		auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
			global_instance, "vkCreateDebugReportCallbackEXT");
		assert(vkCreateDebugReportCallbackEXT != nullptr);

		// Setup the debug report callback
		VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
		debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		debug_report_ci.pfnCallback = debug_report;
		debug_report_ci.pUserData = nullptr;
		err = vkCreateDebugReportCallbackEXT(
			global_instance, &debug_report_ci, global_allocator, &global_debug_report);
		check_vk_result(err);
#else
		// Create Vulkan Instance without any debug feature
		err = vkCreateInstance(&create_info, global_allocator, &global_instance);
		check_vk_result(err);
		IM_UNUSED(global_debug_report);
#endif
	}

	// Select GPU
	{
		uint32_t gpu_count;
		err = vkEnumeratePhysicalDevices(global_instance, &gpu_count, NULL);
		check_vk_result(err);
		assert(gpu_count > 0);

		auto* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
		err = vkEnumeratePhysicalDevices(global_instance, &gpu_count, gpus);
		check_vk_result(err);

		// If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available. This
		// covers most common cases (multi-gpu/integrated+dedicated graphics). Handling more complicated setups
		// (multiple dedicated GPUs) is out of scope of this sample.
		int use_gpu = 0;
		for (int i = 0; i < (int)gpu_count; i++) {
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(gpus[i], &properties);
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				use_gpu = i;
				break;
			}
		}

		global_physical_device = gpus[use_gpu];
		free(gpus);
	}

	// Select graphics queue family
	{
		uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(global_physical_device, &count, NULL);
		auto* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
		vkGetPhysicalDeviceQueueFamilyProperties(global_physical_device, &count, queues);
		for (uint32_t i = 0; i < count; i++)
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				global_queue_family = i;
				break;
			}
		free(queues);
		assert(global_queue_family != (uint32_t)-1);
	}

	// Create Logical Device (with 1 queue)
	{
		int device_extension_count = 2;
		const char* device_extensions[] = { "VK_KHR_swapchain", "VK_KHR_portability_subset" };
		const float queue_priority[] = { 1.0f };
		VkDeviceQueueCreateInfo queue_info[1] = {};
		queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info[0].queueFamilyIndex = global_queue_family;
		queue_info[0].queueCount = 1;
		queue_info[0].pQueuePriorities = queue_priority;
		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
		create_info.pQueueCreateInfos = queue_info;
		create_info.enabledExtensionCount = device_extension_count;
		create_info.ppEnabledExtensionNames = device_extensions;
		err = vkCreateDevice(global_physical_device, &create_info, global_allocator, &global_device);
		check_vk_result(err);
		vkGetDeviceQueue(global_device, global_queue_family, 0, &global_queue);
	}
}

static void cleanup_vulkan()
{
	vkDestroyDescriptorPool(global_device, global_descriptor_pool, global_allocator);

#ifdef IMGUI_VULKAN_DEBUG_REPORT
	// Remove the debug report callback
	auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		global_instance, "vkDestroyDebugReportCallbackEXT");
	vkDestroyDebugReportCallbackEXT(global_instance, global_debug_report, global_allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

	vkDestroyDevice(global_device, global_allocator);
	vkDestroyInstance(global_instance, global_allocator);
}

VkInstance VulkanContext::get_instance() { return global_instance; }

VkAllocationCallbacks* VulkanContext::get_allocator() { return global_allocator; }

uint32_t VulkanContext::get_queue_family() { return global_queue_family; }

VkQueue VulkanContext::get_queue() { return global_queue; }

VkPhysicalDevice VulkanContext::get_physical_device() { return global_physical_device; }

VkDevice VulkanContext::get_device() { return global_device; }

void VulkanContext::cleanup() { cleanup_vulkan(); }

void VulkanContext::construct_and_initialize()
{
	// Setup GLFW window
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		std::cerr << "Could not initalize GLFW!\n";
		return;
	}
	// Setup Vulkan
	if (!glfwVulkanSupported()) {
		std::cerr << "GLFW: Vulkan not supported!\n";
		return;
	}
	uint32_t extensions_count = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
	setup_vulkan(extensions, extensions_count);
}

}