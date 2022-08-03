//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "fg_pch.hpp"

#include "Common.hpp"
#include "GLFW/glfw3.h"
#include "VkBootstrap.h"

#include <unordered_set>

#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanSwapchain.hpp"

namespace ForgottenEngine {

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "Glfw error %d: %s\n", error, description);
}

void VulkanContext::init()
{
	uint32_t extensions_count = 0;
	auto extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
	auto exts = std::unordered_set<const char*>(extensions, extensions + extensions_count);
	exts.insert("VK_EXT_debug_report");
	// exts.insert("VK_KHR_portability_enumeration");
	exts.insert("VK_KHR_get_physical_device_properties2");

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

	auto inst_vkb = inst_ret.value();

	vulkan_instance = inst_vkb.instance;
	// store the debug messenger
	debug_messenger = inst_vkb.debug_messenger;

	physical_device = VulkanPhysicalDevice::select(inst_vkb);

	VkPhysicalDeviceFeatures enabledFeatures{};
	enabledFeatures.samplerAnisotropy = true;
	enabledFeatures.fillModeNonSolid = true;
	enabledFeatures.independentBlend = true;

	device = Reference<VulkanDevice>::create(physical_device, enabledFeatures);

	VulkanAllocator::init(device);

	// Pipeline Cache
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	auto vulkan_device = device->get_vulkan_device();
	VK_CHECK(vkCreatePipelineCache(vulkan_device, &pipelineCacheCreateInfo, nullptr, &pipeline_cache));
}

VulkanContext::~VulkanContext()
{
	device->destroy();
	vkb::destroy_debug_utils_messenger(vulkan_instance, debug_messenger);

	vkDestroyInstance(vulkan_instance, nullptr);
	vulkan_instance = nullptr;
}

Reference<VulkanDevice> VulkanContext::get_device() { return device; }

Reference<VulkanContext> VulkanContext::get() { return { Renderer::get_context() }; }

Reference<VulkanDevice> VulkanContext::get_current_device() { return get()->get_device(); }

}