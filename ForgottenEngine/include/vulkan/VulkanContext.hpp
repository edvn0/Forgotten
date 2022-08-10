#pragma once

#include "Reference.hpp"
#include "render/Renderer.hpp"
#include "render/RendererContext.hpp"
#include "vulkan/VulkanDevice.hpp"
#include "vulkan/VulkanSwapchain.hpp"

#include <functional>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace ForgottenEngine {

	class VulkanContext : public RendererContext {
	public:
		VulkanContext() = default;
		~VulkanContext() override;

		void init() override;

		Reference<VulkanDevice> get_device();

		static VkInstance get_instance() { return vulkan_instance; }

		static Reference<VulkanContext> get();
		static Reference<VulkanDevice> get_current_device();

	private:
		// Devices
		Reference<VulkanPhysicalDevice> physical_device;
		Reference<VulkanDevice> device;

		// Vulkan instance
		inline static VkInstance vulkan_instance;

		VkPipelineCache pipeline_cache = nullptr;
		VulkanSwapchain swapchain;
		VkDebugUtilsMessengerEXT debug_messenger;
	};

} // namespace ForgottenEngine