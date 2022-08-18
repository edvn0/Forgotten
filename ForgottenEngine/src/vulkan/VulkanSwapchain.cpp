#include "fg_pch.hpp"

#include "vulkan/VulkanSwapchain.hpp"

#include "ApplicationProperties.hpp"
#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

	void VulkanSwapchain::init(VkInstance inst)
	{
		instance = inst;
		device = VulkanContext::get_current_device();
	}

	void VulkanSwapchain::init_surface(GLFWwindow* handle)
	{
		VkPhysicalDevice physicalDevice = VulkanContext::get_current_device()->get_physical_device()->get_vulkan_physical_device();

		glfwCreateWindowSurface(instance, handle, nullptr, &surface);

		// Get available queue family properties
		uint32_t queue_count;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_count, nullptr);
		CORE_ASSERT(queue_count >= 1, "Could not create families");

		std::vector<VkQueueFamilyProperties> queue_props(queue_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_count, queue_props.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supportsPresent(queue_count);
		for (uint32_t i = 0; i < queue_count; i++) {
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
		}

		// Search for a graphics and a present queue in the array of queue
		// families, try to find one that supports both
		uint32_t graphics_queue_index = UINT32_MAX;
		uint32_t present_queue_index = UINT32_MAX;
		for (uint32_t i = 0; i < queue_count; i++) {
			if (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				graphics_queue_index = i;
			}
		}
		if (present_queue_index == UINT32_MAX) {
			// If there's no queue that supports both present and graphics
			// try to find a separate present queue
			for (uint32_t i = 0; i < queue_count; ++i) {
				if (supportsPresent[i] == VK_TRUE) {
					present_queue_index = i;
					break;
				}
			}
		}

		CORE_ASSERT_BOOL(graphics_queue_index != UINT32_MAX);
		CORE_ASSERT_BOOL(present_queue_index != UINT32_MAX);

		queue_node_index = graphics_queue_index;

		find_image_format_and_color_space();
	}

	void VulkanSwapchain::create(uint32_t* w, uint32_t* h, bool vsync)
	{
		is_vsync = vsync;

		// VkDevice device = VulkanContext::get_current_device()->get_vulkan_device();
		VkPhysicalDevice physicalDevice = VulkanContext::get_current_device()->get_physical_device()->get_vulkan_physical_device();

		VkSwapchainKHR old_sc = swapchain;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surfCaps;
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps));

		// Get available present modes
		uint32_t presentModeCount;
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr));
		CORE_ASSERT_BOOL(presentModeCount > 0);
		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

		VkExtent2D sc_extent = {};
		// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the
		// swapchain
		if (surfCaps.currentExtent.width == (uint32_t)-1) {
			// If the surface size is undefined, the size is set to
			// the size of the images requested.
			sc_extent.width = *w;
			sc_extent.height = *h;
		} else {
			// If the surface size is defined, the swap chain size must match
			sc_extent = surfCaps.currentExtent;
			*w = surfCaps.currentExtent.width;
			*h = surfCaps.currentExtent.height;
		}

		width = *w;
		height = *h;

		// Select a present mode for the swapchain

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR sc_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!vsync) {
			for (size_t i = 0; i < presentModeCount; i++) {
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
					sc_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
					sc_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				}
			}
		}

		constexpr auto present_mode_to_string = [](const VkPresentModeKHR& present_mode) {
			switch (present_mode) {
			case VK_PRESENT_MODE_IMMEDIATE_KHR:
				return "Immediate";
			case VK_PRESENT_MODE_MAILBOX_KHR:
				return "Mailbox";
			case VK_PRESENT_MODE_FIFO_KHR:
				return "Fifo";
			case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
				return "Fifo Relaxed";
			case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
				return "Shared Demand Refresh";
			case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
				return "Shared Continuous Refresh";
			case VK_PRESENT_MODE_MAX_ENUM_KHR:
				return "Max enum";
			}
		};
		CORE_DEBUG("Chose [{}] as present mode.", present_mode_to_string(sc_present_mode));

		// Determine the number of images
		uint32_t asked_sc_images = surfCaps.minImageCount + 1;
		if ((surfCaps.maxImageCount > 0) && (asked_sc_images > surfCaps.maxImageCount)) {
			asked_sc_images = surfCaps.maxImageCount;
		}

		CORE_DEBUG("Asked Swapchain for [{}] images.", asked_sc_images);

		// Find the transformation of the surface
		VkSurfaceTransformFlagsKHR pre_transform;
		if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			// We prefer a non-rotated transform
			pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		} else {
			pre_transform = surfCaps.currentTransform;
		}

		// Find a supported composite alpha format (not all devices support alpha opaque)
		VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		// Simply select the first composite alpha format available
		std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
			VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
			VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (auto& compositeAlphaFlag : compositeAlphaFlags) {
			if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
				composite_alpha = compositeAlphaFlag;
				break;
			};
		}

		VkSwapchainCreateInfoKHR swapchain_ci = {};
		swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_ci.pNext = nullptr;
		swapchain_ci.surface = surface;
		swapchain_ci.minImageCount = asked_sc_images;
		swapchain_ci.imageFormat = color_format;
		swapchain_ci.imageColorSpace = color_space;
		swapchain_ci.imageExtent = { sc_extent.width, sc_extent.height };
		swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_ci.preTransform = (VkSurfaceTransformFlagBitsKHR)pre_transform;
		swapchain_ci.imageArrayLayers = 1;
		swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_ci.queueFamilyIndexCount = 0;
		swapchain_ci.pQueueFamilyIndices = nullptr;
		swapchain_ci.presentMode = sc_present_mode;
		swapchain_ci.oldSwapchain = old_sc;
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		swapchain_ci.clipped = VK_TRUE;
		swapchain_ci.compositeAlpha = composite_alpha;

		// Enable transfer source on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
			swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		// Enable transfer destination on swap chain images if supported
		if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
			swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		VK_CHECK(vkCreateSwapchainKHR(device->get_vulkan_device(), &swapchain_ci, nullptr, &swapchain));

		if (old_sc)
			vkDestroySwapchainKHR(device->get_vulkan_device(), old_sc, nullptr);

		for (auto& image : images)
			vkDestroyImageView(device->get_vulkan_device(), image.view, nullptr);
		images.clear();

		VK_CHECK(vkGetSwapchainImagesKHR(device->get_vulkan_device(), swapchain, &image_count, nullptr));
		// Get the swap chain images
		images.resize(image_count);
		vulkan_images.resize(image_count);
		VK_CHECK(vkGetSwapchainImagesKHR(device->get_vulkan_device(), swapchain, &image_count, vulkan_images.data()));

		// Get the swap chain buffers containing the image and imageview
		images.resize(image_count);
		for (uint32_t i = 0; i < image_count; i++) {
			VkImageViewCreateInfo cav = {};
			cav.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			cav.pNext = nullptr;
			cav.format = color_format;
			cav.image = vulkan_images[i];
			cav.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
			cav.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			cav.subresourceRange.baseMipLevel = 0;
			cav.subresourceRange.levelCount = 1;
			cav.subresourceRange.baseArrayLayer = 0;
			cav.subresourceRange.layerCount = 1;
			cav.viewType = VK_IMAGE_VIEW_TYPE_2D;
			cav.flags = 0;

			images[i].image = vulkan_images[i];

			VK_CHECK(vkCreateImageView(device->get_vulkan_device(), &cav, nullptr, &images[i].view));
		}

		// Create command buffers
		{
			for (auto& cmd_buffer : command_buffers)
				vkDestroyCommandPool(device->get_vulkan_device(), cmd_buffer.command_pool, nullptr);

			VkCommandPoolCreateInfo pci = {};
			pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			pci.queueFamilyIndex = queue_node_index;
			pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

			VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandBufferCount = 1;

			command_buffers.resize(image_count);
			for (auto& cmd_buffer : command_buffers) {
				VK_CHECK(vkCreateCommandPool(device->get_vulkan_device(), &pci, nullptr, &cmd_buffer.command_pool));

				commandBufferAllocateInfo.commandPool = cmd_buffer.command_pool;
				VK_CHECK(vkAllocateCommandBuffers(device->get_vulkan_device(), &commandBufferAllocateInfo, &cmd_buffer.buffer));
			}
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Synchronization Objects
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (!semaphores.render_complete_semaphore || !semaphores.present_complete_semaphore) {
			VkSemaphoreCreateInfo semaphoreCreateInfo {};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VK_CHECK(vkCreateSemaphore(
				VulkanContext::get_current_device()->get_vulkan_device(), &semaphoreCreateInfo, nullptr, &semaphores.render_complete_semaphore));
			VK_CHECK(vkCreateSemaphore(
				VulkanContext::get_current_device()->get_vulkan_device(), &semaphoreCreateInfo, nullptr, &semaphores.present_complete_semaphore));
		}

		if (wait_fences.size() != image_count) {
			VkFenceCreateInfo fenceCreateInfo {};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			wait_fences.resize(image_count);
			for (auto& fence : wait_fences) {
				VK_CHECK(vkCreateFence(VulkanContext::get_current_device()->get_vulkan_device(), &fenceCreateInfo, nullptr, &fence));
			}
		}

		VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pWaitDstStageMask = &pipelineStageFlags;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &semaphores.present_complete_semaphore;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &semaphores.render_complete_semaphore;

		// Render Pass
		VkAttachmentDescription colorAttachmentDesc = {};
		// Color attachment
		colorAttachmentDesc.format = color_format;
		colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorReference = {};
		colorReference.attachment = 0;
		colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;
		subpassDescription.pResolveAttachments = nullptr;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachmentDesc;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpassDescription;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VK_CHECK(vkCreateRenderPass(VulkanContext::get_current_device()->get_vulkan_device(), &renderPassInfo, nullptr, &render_pass));

		// Create framebuffers for every swapchain image
		{
			for (auto& framebuffer : framebuffers)
				vkDestroyFramebuffer(device->get_vulkan_device(), framebuffer, nullptr);

			VkFramebufferCreateInfo frameBufferCreateInfo = {};
			frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			frameBufferCreateInfo.renderPass = render_pass;
			frameBufferCreateInfo.attachmentCount = 1;
			frameBufferCreateInfo.width = width;
			frameBufferCreateInfo.height = height;
			frameBufferCreateInfo.layers = 1;

			framebuffers.resize(image_count);
			for (uint32_t i = 0; i < framebuffers.size(); i++) {
				frameBufferCreateInfo.pAttachments = &images[i].view;
				VK_CHECK(
					vkCreateFramebuffer(VulkanContext::get_current_device()->get_vulkan_device(), &frameBufferCreateInfo, nullptr, &framebuffers[i]));
			}
		}
	}

	void VulkanSwapchain::destroy()
	{
		auto vk_device = device->get_vulkan_device();
		vkDeviceWaitIdle(vk_device);

		if (swapchain)
			vkDestroySwapchainKHR(vk_device, swapchain, nullptr);

		for (auto& image : images)
			vkDestroyImageView(vk_device, image.view, nullptr);

		for (auto& cmd_buffer : command_buffers)
			vkDestroyCommandPool(vk_device, cmd_buffer.command_pool, nullptr);

		if (render_pass)
			vkDestroyRenderPass(vk_device, render_pass, nullptr);

		for (auto framebuffer : framebuffers)
			vkDestroyFramebuffer(vk_device, framebuffer, nullptr);

		if (semaphores.render_complete_semaphore)
			vkDestroySemaphore(vk_device, semaphores.render_complete_semaphore, nullptr);

		if (semaphores.present_complete_semaphore)
			vkDestroySemaphore(vk_device, semaphores.present_complete_semaphore, nullptr);

		for (auto& fence : wait_fences)
			vkDestroyFence(vk_device, fence, nullptr);

		vkDeviceWaitIdle(vk_device);
	}

	void VulkanSwapchain::on_resize(uint32_t resize_w, uint32_t resize_h)
	{
		vkDeviceWaitIdle(device->get_vulkan_device());
		create(&resize_w, &resize_h, is_vsync);
		vkDeviceWaitIdle(device->get_vulkan_device());
	}

	void VulkanSwapchain::begin_frame()
	{

		// Resource release queue
		auto& queue = Renderer::get_render_resource_free_queue(current_buffer_index);
		queue.execute();

		current_image_index = acquire_next_image();

		VK_CHECK(vkResetCommandPool(VulkanContext::get_current_device()->get_vulkan_device(), command_buffers[current_buffer_index].command_pool, 0));
	}

	void VulkanSwapchain::present()
	{
		static constexpr uint64_t DEFAULT_FENCE_TIMEOUT = 100000000000;

		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pWaitDstStageMask = &waitStageMask;
		submit_info.pWaitSemaphores = &semaphores.present_complete_semaphore;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &semaphores.render_complete_semaphore;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pCommandBuffers = &command_buffers[current_buffer_index].buffer;
		submit_info.commandBufferCount = 1;

		VK_CHECK(vkResetFences(VulkanContext::get_current_device()->get_vulkan_device(), 1, &wait_fences[current_buffer_index]));
		VK_CHECK(vkQueueSubmit(VulkanContext::get_current_device()->get_graphics_queue(), 1, &submit_info, wait_fences[current_buffer_index]));

		VkResult result;
		{
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.pNext = nullptr;
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = &swapchain;
			presentInfo.pImageIndices = &current_image_index;

			presentInfo.pWaitSemaphores = &semaphores.render_complete_semaphore;
			presentInfo.waitSemaphoreCount = 1;
			result = vkQueuePresentKHR(VulkanContext::get_current_device()->get_graphics_queue(), &presentInfo);
		}

		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				CORE_INFO("RESIZE");
				on_resize(width, height);
				return;
			} else {
				VK_CHECK(result);
			}
		}

		{
			const auto& config = Renderer::get_config();
			current_buffer_index = (current_buffer_index + 1) % config.frames_in_flight;
			// Make sure the frame we're requesting has finished rendering
			VK_CHECK(vkWaitForFences(
				VulkanContext::get_current_device()->get_vulkan_device(), 1, &wait_fences[current_buffer_index], VK_TRUE, UINT64_MAX));
		}
	}

	uint32_t VulkanSwapchain::acquire_next_image()
	{
		uint32_t imageIndex;
		VK_CHECK(vkAcquireNextImageKHR(VulkanContext::get_current_device()->get_vulkan_device(), swapchain, UINT64_MAX,
			semaphores.present_complete_semaphore, (VkFence) nullptr, &imageIndex));
		CORE_DEBUG("{}", imageIndex);
		return imageIndex;
	}

	void VulkanSwapchain::find_image_format_and_color_space()
	{
		VkPhysicalDevice physical_device = VulkanContext::get_current_device()->get_physical_device()->get_vulkan_physical_device();

		// Get list of supported surface formats
		uint32_t formatCount;
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, nullptr));
		CORE_ASSERT_BOOL(formatCount > 0);

		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, surfaceFormats.data()));

		// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
		// there is no preferered format, so we assume VK_FORMAT_B8G8R8A8_UNORM
		if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED)) {
			color_format = VK_FORMAT_B8G8R8A8_UNORM;
			color_space = surfaceFormats[0].colorSpace;
		} else {
			// iterate over the list of available surface format and
			// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
			bool found_B8G8R8A8_UNORM = false;
			for (auto&& surfaceFormat : surfaceFormats) {
				if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
					color_format = surfaceFormat.format;
					color_space = surfaceFormat.colorSpace;
					found_B8G8R8A8_UNORM = true;
					break;
				}
			}

			// in case VK_FORMAT_B8G8R8A8_UNORM is not available
			// select the first available color format
			if (!found_B8G8R8A8_UNORM) {
				color_format = surfaceFormats[0].format;
				color_space = surfaceFormats[0].colorSpace;
			}
		}
	}

} // namespace ForgottenEngine
