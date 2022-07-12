//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "fg_pch.hpp"

#include "Common.hpp"
#include "GLFW/glfw3.h"
#include "VkBootstrap.h"
#include "vulkan/VulkanContext.hpp"

namespace ForgottenEngine {

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw error %d: %s\n", error, description);
}

#define IMGUI_VULKAN_DEBUG_REPORT

static VkDebugUtilsMessengerEXT global_debug_messenger = nullptr;
static VkAllocationCallbacks* global_allocator = nullptr;
static VkInstance global_instance = VK_NULL_HANDLE;
static VkSurfaceKHR global_surface = VK_NULL_HANDLE;
static VkPhysicalDevice global_physical_device = VK_NULL_HANDLE;
static VkDevice global_device = VK_NULL_HANDLE;
static uint32_t global_queue_family = (uint32_t)-1;
static VkQueue global_queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT global_debug_report = VK_NULL_HANDLE;
static GLFWwindow* global_window_handle = nullptr;

static uint32_t minimum_alignment = 0;

void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

static void cleanup_vulkan()
{
#ifdef IMGUI_VULKAN_DEBUG_REPORT
	// Remove the debug report callback
	auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
		global_instance, "vkDestroyDebugReportCallbackEXT");
	vkDestroyDebugReportCallbackEXT(global_instance, global_debug_report, global_allocator);
#endif // IMGUI_VULKAN_DEBUG_REPORT

	vkDestroyDevice(global_device, nullptr);
	vkDestroySurfaceKHR(global_instance, global_surface, nullptr);
	vkb::destroy_debug_utils_messenger(global_instance, global_debug_messenger);
	vkDestroyInstance(global_instance, nullptr);
}

VkInstance VulkanContext::get_instance() { return global_instance; }

VkSurfaceKHR VulkanContext::get_surface() { return global_surface; }

GLFWwindow* VulkanContext::get_window_handle() { return global_window_handle; }

VkAllocationCallbacks* VulkanContext::get_allocator() { return global_allocator; }

uint32_t VulkanContext::get_queue_family() { return global_queue_family; }

VkQueue VulkanContext::get_queue() { return global_queue; }

VkPhysicalDevice VulkanContext::get_physical_device() { return global_physical_device; }

VkDevice VulkanContext::get_device() { return global_device; }

uint32_t VulkanContext::get_alignment() { return minimum_alignment; }

std::pair<int, int> VulkanContext::get_framebuffer_size()
{
	int width, height;
	glfwGetFramebufferSize(global_window_handle, &width, &height);

	return { width, height };
}

void VulkanContext::cleanup() { cleanup_vulkan(); }

void VulkanContext::construct_and_initialize(GLFWwindow* handle)
{
	global_window_handle = handle;

	uint32_t extensions_count = 0;
	auto extensions = glfwGetRequiredInstanceExtensions(&extensions_count);

	auto exts = std::vector<const char*>(extensions, extensions + extensions_count);
	exts.push_back("VK_EXT_debug_report");
	exts.push_back("VK_KHR_portability_enumeration");
	exts.push_back("VK_KHR_get_physical_device_properties2");

	vkb::InstanceBuilder builder;

	// make the Vulkan instance, with basic debug features
	builder.set_app_name("ForgottenEngine")
		.request_validation_layers(true)
		.require_api_version(1, 1, 0)
		.use_default_debug_messenger();

	for (const auto& ext : exts) {
		builder.enable_extension(ext);
	}

	auto inst_ret = builder.build();

	vkb::Instance vkb_inst = inst_ret.value();

	// store the instance
	global_instance = vkb_inst.instance;
	// store the debug messenger
	global_debug_messenger = vkb_inst.debug_messenger;

	// use vkbootstrap to select a GPU.
	VK_CHECK(glfwCreateWindowSurface(global_instance, handle, nullptr, &global_surface));
	auto surface = VulkanContext::get_surface();

	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physical_device = selector.set_minimum_version(1, 1).set_surface(surface).select().value();

	// create the final Vulkan device
	VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features = {};
	shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
	shader_draw_parameters_features.pNext = nullptr;
	shader_draw_parameters_features.shaderDrawParameters = VK_TRUE;
	vkb::DeviceBuilder device_builder{ physical_device };
	device_builder.add_pNext(&shader_draw_parameters_features);

	vkb::Device vkb_device = device_builder.build().value();

	minimum_alignment = vkb_device.physical_device.properties.limits.minUniformBufferOffsetAlignment;

	CORE_DEBUG("GPU Minimum Buffer Alignment: {}",
		vkb_device.physical_device.properties.limits.minUniformBufferOffsetAlignment);

	// Get the VkDevice handle used in the rest of a Vulkan application
	global_device = vkb_device.device;
	global_physical_device = physical_device.physical_device;

	// use vk-bootstrap to get a Graphics queue
	global_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	global_queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();
}

float VulkanContext::get_dpi()
{
	float xscale, yscale;
	glfwGetMonitorContentScale(glfwGetPrimaryMonitor(), &xscale, &yscale);
	return xscale;
}

}