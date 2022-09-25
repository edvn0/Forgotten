//
// Created by Edwin Carlsson on 2022-07-01.
//

#include "fg_pch.hpp"

#include "vulkan/VulkanContext.hpp"

#include "Common.hpp"
#include "GLFW/glfw3.h"
#include "render/Renderer.hpp"
#include "vulkan/VulkanAllocator.hpp"
#include "vulkan/VulkanDevice.hpp"

#include <unordered_set>
#include <vector>
#include <vulkan/VulkanImage.hpp>

namespace ForgottenEngine {

	namespace Utils {
		inline const char* VKResultToString(VkResult result)
		{
			switch (result) {
			case VK_SUCCESS:
				return "VK_SUCCESS";
			case VK_NOT_READY:
				return "VK_NOT_READY";
			case VK_TIMEOUT:
				return "VK_TIMEOUT";
			case VK_EVENT_SET:
				return "VK_EVENT_SET";
			case VK_EVENT_RESET:
				return "VK_EVENT_RESET";
			case VK_INCOMPLETE:
				return "VK_INCOMPLETE";
			case VK_ERROR_OUT_OF_HOST_MEMORY:
				return "VK_ERROR_OUT_OF_HOST_MEMORY";
			case VK_ERROR_OUT_OF_DEVICE_MEMORY:
				return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
			case VK_ERROR_INITIALIZATION_FAILED:
				return "VK_ERROR_INITIALIZATION_FAILED";
			case VK_ERROR_DEVICE_LOST:
				return "VK_ERROR_DEVICE_LOST";
			case VK_ERROR_MEMORY_MAP_FAILED:
				return "VK_ERROR_MEMORY_MAP_FAILED";
			case VK_ERROR_LAYER_NOT_PRESENT:
				return "VK_ERROR_LAYER_NOT_PRESENT";
			case VK_ERROR_EXTENSION_NOT_PRESENT:
				return "VK_ERROR_EXTENSION_NOT_PRESENT";
			case VK_ERROR_FEATURE_NOT_PRESENT:
				return "VK_ERROR_FEATURE_NOT_PRESENT";
			case VK_ERROR_INCOMPATIBLE_DRIVER:
				return "VK_ERROR_INCOMPATIBLE_DRIVER";
			case VK_ERROR_TOO_MANY_OBJECTS:
				return "VK_ERROR_TOO_MANY_OBJECTS";
			case VK_ERROR_FORMAT_NOT_SUPPORTED:
				return "VK_ERROR_FORMAT_NOT_SUPPORTED";
			case VK_ERROR_FRAGMENTED_POOL:
				return "VK_ERROR_FRAGMENTED_POOL";
			case VK_ERROR_UNKNOWN:
				return "VK_ERROR_UNKNOWN";
			case VK_ERROR_OUT_OF_POOL_MEMORY:
				return "VK_ERROR_OUT_OF_POOL_MEMORY";
			case VK_ERROR_INVALID_EXTERNAL_HANDLE:
				return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
			case VK_ERROR_FRAGMENTATION:
				return "VK_ERROR_FRAGMENTATION";
			case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:
				return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
			case VK_ERROR_SURFACE_LOST_KHR:
				return "VK_ERROR_SURFACE_LOST_KHR";
			case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
				return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
			case VK_SUBOPTIMAL_KHR:
				return "VK_SUBOPTIMAL_KHR";
			case VK_ERROR_OUT_OF_DATE_KHR:
				return "VK_ERROR_OUT_OF_DATE_KHR";
			case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
				return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
			case VK_ERROR_VALIDATION_FAILED_EXT:
				return "VK_ERROR_VALIDATION_FAILED_EXT";
			case VK_ERROR_INVALID_SHADER_NV:
				return "VK_ERROR_INVALID_SHADER_NV";
			case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
				return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
			case VK_ERROR_NOT_PERMITTED_EXT:
				return "VK_ERROR_NOT_PERMITTED_EXT";
			case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
				return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
			case VK_THREAD_IDLE_KHR:
				return "VK_THREAD_IDLE_KHR";
			case VK_THREAD_DONE_KHR:
				return "VK_THREAD_DONE_KHR";
			case VK_OPERATION_DEFERRED_KHR:
				return "VK_OPERATION_DEFERRED_KHR";
			case VK_OPERATION_NOT_DEFERRED_KHR:
				return "VK_OPERATION_NOT_DEFERRED_KHR";
			case VK_PIPELINE_COMPILE_REQUIRED_EXT:
				return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
			case VK_RESULT_MAX_ENUM:
				return "VK_RESULT_MAX_ENUM";
			default:
				core_assert_bool(false);
			}
		}

		inline const char* VkObjectTypeToString(const VkObjectType type)
		{
			switch (type) {
			case VK_OBJECT_TYPE_COMMAND_BUFFER:
				return "VK_OBJECT_TYPE_COMMAND_BUFFER";
			case VK_OBJECT_TYPE_PIPELINE:
				return "VK_OBJECT_TYPE_PIPELINE";
			case VK_OBJECT_TYPE_FRAMEBUFFER:
				return "VK_OBJECT_TYPE_FRAMEBUFFER";
			case VK_OBJECT_TYPE_IMAGE:
				return "VK_OBJECT_TYPE_IMAGE";
			case VK_OBJECT_TYPE_QUERY_POOL:
				return "VK_OBJECT_TYPE_QUERY_POOL";
			case VK_OBJECT_TYPE_RENDER_PASS:
				return "VK_OBJECT_TYPE_RENDER_PASS";
			case VK_OBJECT_TYPE_COMMAND_POOL:
				return "VK_OBJECT_TYPE_COMMAND_POOL";
			case VK_OBJECT_TYPE_PIPELINE_CACHE:
				return "VK_OBJECT_TYPE_PIPELINE_CACHE";
			case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR:
				return "VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR";
			case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV:
				return "VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_NV";
			case VK_OBJECT_TYPE_BUFFER:
				return "VK_OBJECT_TYPE_BUFFER";
			case VK_OBJECT_TYPE_BUFFER_VIEW:
				return "VK_OBJECT_TYPE_BUFFER_VIEW";
			case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
				return "VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT";
			case VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT:
				return "VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT";
			case VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR:
				return "VK_OBJECT_TYPE_DEFERRED_OPERATION_KHR";
			case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
				return "VK_OBJECT_TYPE_DESCRIPTOR_POOL";
			case VK_OBJECT_TYPE_DESCRIPTOR_SET:
				return "VK_OBJECT_TYPE_DESCRIPTOR_SET";
			case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
				return "VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT";
			case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE:
				return "VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE";
			case VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT:
				return "VK_OBJECT_TYPE_PRIVATE_DATA_SLOT_EXT";
			case VK_OBJECT_TYPE_DEVICE:
				return "VK_OBJECT_TYPE_DEVICE";
			case VK_OBJECT_TYPE_DEVICE_MEMORY:
				return "VK_OBJECT_TYPE_DEVICE_MEMORY";
			case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
				return "VK_OBJECT_TYPE_PIPELINE_LAYOUT";
			case VK_OBJECT_TYPE_DISPLAY_KHR:
				return "VK_OBJECT_TYPE_DISPLAY_KHR";
			case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
				return "VK_OBJECT_TYPE_DISPLAY_MODE_KHR";
			case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
				return "VK_OBJECT_TYPE_PHYSICAL_DEVICE";
			case VK_OBJECT_TYPE_EVENT:
				return "VK_OBJECT_TYPE_EVENT";
			case VK_OBJECT_TYPE_FENCE:
				return "VK_OBJECT_TYPE_FENCE";
			case VK_OBJECT_TYPE_IMAGE_VIEW:
				return "VK_OBJECT_TYPE_IMAGE_VIEW";
			case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV:
				return "VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NV";
			case VK_OBJECT_TYPE_INSTANCE:
				return "VK_OBJECT_TYPE_INSTANCE";
			case VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL:
				return "VK_OBJECT_TYPE_PERFORMANCE_CONFIGURATION_INTEL";
			case VK_OBJECT_TYPE_QUEUE:
				return "VK_OBJECT_TYPE_QUEUE";
			case VK_OBJECT_TYPE_SAMPLER:
				return "VK_OBJECT_TYPE_SAMPLER";
			case VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION:
				return "VK_OBJECT_TYPE_SAMPLER_YCBCR_CONVERSION";
			case VK_OBJECT_TYPE_SEMAPHORE:
				return "VK_OBJECT_TYPE_SEMAPHORE";
			case VK_OBJECT_TYPE_SHADER_MODULE:
				return "VK_OBJECT_TYPE_SHADER_MODULE";
			case VK_OBJECT_TYPE_SURFACE_KHR:
				return "VK_OBJECT_TYPE_SURFACE_KHR";
			case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
				return "VK_OBJECT_TYPE_SWAPCHAIN_KHR";
			case VK_OBJECT_TYPE_VALIDATION_CACHE_EXT:
				return "VK_OBJECT_TYPE_VALIDATION_CACHE_EXT";
			case VK_OBJECT_TYPE_UNKNOWN:
				return "VK_OBJECT_TYPE_UNKNOWN";
			case VK_OBJECT_TYPE_MAX_ENUM:
				return "VK_OBJECT_TYPE_MAX_ENUM";
			default:
				core_assert_bool(false);
			}
		}

	} // namespace Utils

	constexpr const char* VkDebugUtilsMessageType(const VkDebugUtilsMessageTypeFlagsEXT type)
	{
		switch (type) {
		case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
			return "General";
		case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
			return "Validation";
		case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
			return "Performance";
		default:
			return "Unknown";
		}
	}

	constexpr const char* VkDebugUtilsMessageSeverity(const VkDebugUtilsMessageSeverityFlagBitsEXT severity)
	{
		switch (severity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			return "error";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			return "warning";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			return "info";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			return "verbose";
		default:
			return "unknown";
		}
	}

	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsMessengerCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		const VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		(void)pUserData; // Unused argument

		const bool performanceWarnings = false;
		if (!performanceWarnings) {
			if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
				return VK_FALSE;
		}

		std::string labels, objects;
		if (pCallbackData->cmdBufLabelCount) {
			labels = fmt::format("\tLabels({}): \n", pCallbackData->cmdBufLabelCount);
			for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; ++i) {
				const auto& label = pCallbackData->pCmdBufLabels[i];
				const std::string colorStr = fmt::format("[ {}, {}, {}, {} ]", label.color[0], label.color[1], label.color[2], label.color[3]);
				labels.append(fmt::format(
					"\t\t- Command Buffer Label[{0}]: name: {1}, color: {2}\n", i, label.pLabelName ? label.pLabelName : "NULL", colorStr));
			}
		}

		if (pCallbackData->objectCount) {
			objects = fmt::format("\tObjects({}): \n", pCallbackData->objectCount);
			for (uint32_t i = 0; i < pCallbackData->objectCount; ++i) {
				const auto& object = pCallbackData->pObjects[i];
				objects.append(fmt::format("\t\t- Object[{0}] name: {1}, type: {2}, handle: {3:#x}\n", i,
					object.pObjectName ? object.pObjectName : "NULL", Utils::VkObjectTypeToString(object.objectType), object.objectHandle));
			}
		}

		CORE_WARN("{0} {1} message: \n\t{2}\n {3} {4}", VkDebugUtilsMessageType(messageType), VkDebugUtilsMessageSeverity(messageSeverity),
			pCallbackData->pMessage, labels, objects);

		return VK_FALSE;
	}

	static void glfw_error_callback(int error, const char* description) { fprintf(stderr, "Glfw error %d: %s\n", error, description); }

	void VulkanContext::init()
	{
		VkApplicationInfo app_info = {};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Forgotten";
		app_info.pEngineName = "ForgottenEngine";
		app_info.apiVersion = VK_API_VERSION_1_2;

		VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT };
		VkValidationFeaturesEXT features = {};
		features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
		features.enabledValidationFeatureCount = 1;
		features.pEnabledValidationFeatures = enables;

		uint32_t extensions_count = 0;
		auto extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
		auto exts = std::unordered_set<const char*>(extensions, extensions + extensions_count);

#ifdef FORGOTTEN_WINDOWS
		exts.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		exts.insert(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#elif defined(FORGOTTEN_MACOS)
		exts.insert("VK_EXT_debug_report");
		exts.insert("VK_KHR_portability_enumeration");
		exts.insert("VK_KHR_get_physical_device_properties2");
#endif
		std::vector<const char*> out(exts.begin(), exts.end());

		for (auto& inst : out) {
			CORE_INFO("{}", inst);
		}

		VkInstanceCreateInfo create_fino = {};
		create_fino.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_fino.pNext = nullptr; //;&features;
		create_fino.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		create_fino.pApplicationInfo = &app_info;
		create_fino.enabledExtensionCount = (uint32_t)out.size();
		create_fino.ppEnabledExtensionNames = out.data();

		const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool validationLayerPresent = false;
		CORE_TRACE("Vulkan Instance Layers:");
		for (const auto& layer : instanceLayerProperties) {
			CORE_TRACE("\t\t{0}", layer.layerName);
			if (strcmp(layer.layerName, validationLayerName) == 0) {
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent) {
			create_fino.ppEnabledLayerNames = &validationLayerName;
			create_fino.enabledLayerCount = 1;
		} else {
			CORE_ERROR("Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled");
		}

		vk_check(vkCreateInstance(&create_fino, nullptr, &vulkan_instance));

		physical_device = VulkanPhysicalDevice::select();

		VkPhysicalDeviceFeatures enabledFeatures {};
		enabledFeatures.samplerAnisotropy = true;
		enabledFeatures.fillModeNonSolid = true;
		enabledFeatures.independentBlend = true;

#ifdef FORGOTTEN_WINDOWS
		enabledFeatures.pipelineStatisticsQuery = true;
		enabledFeatures.wideLines = true;
#endif

		device = Reference<VulkanDevice>::create(physical_device, enabledFeatures);

		VulkanAllocator::init(device);

		// Pipeline Cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		auto vulkan_device = device->get_vulkan_device();
		vk_check(vkCreatePipelineCache(vulkan_device, &pipelineCacheCreateInfo, nullptr, &pipeline_cache));
	}

	VulkanContext::~VulkanContext()
	{
		device->destroy();

		vkDestroyInstance(vulkan_instance, nullptr);
		vulkan_instance = nullptr;
	}

	Reference<VulkanDevice> VulkanContext::get_device() { return device; }

	Reference<VulkanContext> VulkanContext::get() { return { Renderer::get_context() }; }

	Reference<VulkanDevice> VulkanContext::get_current_device() { return get()->get_device(); }

} // namespace ForgottenEngine