#include "fg_pch.hpp"

#include "vulkan/VulkanSwapchain.hpp"

#include "ApplicationProperties.hpp"
#include "render/Renderer.hpp"
#include "vulkan/VulkanContext.hpp"

#include <GLFW/glfw3.h>

namespace ForgottenEngine {

	static constexpr uint64_t default_fence_timeout = 100000000000;

	static VkDevice get_device()
	{
		static VkDevice device;

		if (!device) {
			device = VulkanContext::get_current_device()->get_vulkan_device();
		}

		return device;
	}

	void VulkanSwapchain::init_surface(GLFWwindow* handle)
	{
		VkPhysicalDevice physical_device = VulkanContext::get_current_device()->get_physical_device()->get_vulkan_physical_device();

		glfwCreateWindowSurface(VulkanContext::get_instance(), handle, nullptr, &surface);

		// Get available queue family properties
		uint32_t queue_count;
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, nullptr);
		core_assert(queue_count >= 1, "Could not create families");

		std::vector<VkQueueFamilyProperties> queue_props(queue_count);
		vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queue_props.data());

		// Iterate over each queue to learn whether it supports presenting:
		// Find a queue with present support
		// Will be used to present the swap chain images to the windowing system
		std::vector<VkBool32> supports_presentation(queue_count);
		for (uint32_t i = 0; i < queue_count; i++) {
			vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &supports_presentation[i]);
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
				if (supports_presentation[i] == VK_TRUE) {
					present_queue_index = i;
					break;
				}
			}
		}

		core_assert_bool(graphics_queue_index != UINT32_MAX);
		core_assert_bool(present_queue_index != UINT32_MAX);

		queue_node_index = graphics_queue_index;

		find_image_format_and_color_space();
	}

	void VulkanSwapchain::create(uint32_t* w, uint32_t* h, bool vsync)
	{
		is_vsync = vsync;

		// VkDevice device = get_device();
		VkPhysicalDevice physical_device = VulkanContext::get_current_device()->get_physical_device()->get_vulkan_physical_device();

		VkSwapchainKHR old_sc = swapchain;

		// Get physical device surface properties and formats
		VkSurfaceCapabilitiesKHR surf_caps;
		vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surf_caps));

		// Get available present modes
		uint32_t present_mode_count;
		vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));
		core_assert_bool(present_mode_count > 0);
		std::vector<VkPresentModeKHR> present_modes(present_mode_count);
		vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes.data()));

		VkExtent2D sc_extent = {};
		// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the
		// swapchain
		auto constexpr extent_is_undefined = [](const VkExtent2D& extent2D) -> bool { return extent2D.width == (uint32_t)-1; };

		if (extent_is_undefined(surf_caps.currentExtent)) {
			// If the surface size is undefined, the size is set to
			// the size of the images requested.
			sc_extent.width = *w;
			sc_extent.height = *h;
		} else {
			// If the surface size is defined, the swap chain size must match
			sc_extent = surf_caps.currentExtent;
			*w = surf_caps.currentExtent.width;
			*h = surf_caps.currentExtent.height;
		}

		width = *w;
		height = *h;

		// Select a present mode for the swapchain

		// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
		// This mode waits for the vertical blank ("v-sync")
		VkPresentModeKHR sc_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		// If v-sync is not requested, try to find a mailbox mode
		// It's the lowest latency non-tearing present mode available
		if (!is_vsync) {
			for (size_t i = 0; i < present_mode_count; i++) {
				if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
					sc_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
					break;
				}
				if (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
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
			default:
				core_assert(false, "Could not find chosen present mode.");
			}
		};
		CORE_DEBUG("Chose [{}] as present mode.", present_mode_to_string(sc_present_mode));

		// Determine the number of images
		uint32_t asked_sc_images = surf_caps.minImageCount + 1;
		if ((surf_caps.maxImageCount > 0) && (asked_sc_images > surf_caps.maxImageCount)) {
			asked_sc_images = surf_caps.maxImageCount;
		}

		CORE_DEBUG("Asked Swapchain for [{}] images.", asked_sc_images);

		// Find the transformation of the surface
		VkSurfaceTransformFlagsKHR pre_transform;
		if (surf_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
			// We prefer a non-rotated transform
			pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		} else {
			pre_transform = surf_caps.currentTransform;
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
			if (surf_caps.supportedCompositeAlpha & compositeAlphaFlag) {
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
		if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
			swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}

		// Enable transfer destination on swap chain images if supported
		if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
			swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}

		vk_check(vkCreateSwapchainKHR(get_device(), &swapchain_ci, nullptr, &swapchain));

		if (old_sc)
			vkDestroySwapchainKHR(get_device(), old_sc, nullptr);

		for (auto& image : images)
			vkDestroyImageView(get_device(), image.view, nullptr);
		images.clear();

		vk_check(vkGetSwapchainImagesKHR(get_device(), swapchain, &image_count, nullptr));

		// Get the swap chain images
		images.resize(image_count);
		vulkan_images.resize(image_count);
		vk_check(vkGetSwapchainImagesKHR(get_device(), swapchain, &image_count, vulkan_images.data()));

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

			vk_check(vkCreateImageView(get_device(), &cav, nullptr, &images[i].view));
		}

		// Create command buffers
		{
			for (auto& cmd_buffer : command_buffers)
				vkDestroyCommandPool(get_device(), cmd_buffer.command_pool, nullptr);

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
				vk_check(vkCreateCommandPool(get_device(), &pci, nullptr, &cmd_buffer.command_pool));

				commandBufferAllocateInfo.commandPool = cmd_buffer.command_pool;
				vk_check(vkAllocateCommandBuffers(get_device(), &commandBufferAllocateInfo, &cmd_buffer.buffer));
			}
		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		// Synchronization Objects
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (!semaphores.render_complete_semaphore || !semaphores.present_complete_semaphore) {
			VkSemaphoreCreateInfo semaphore_create_info {};
			semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			vk_check(vkCreateSemaphore(get_device(), &semaphore_create_info, nullptr, &semaphores.render_complete_semaphore));
			vk_check(vkCreateSemaphore(get_device(), &semaphore_create_info, nullptr, &semaphores.present_complete_semaphore));
		}

		if (wait_fences.size() != image_count) {
			VkFenceCreateInfo fence_create_info {};
			fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			wait_fences.resize(image_count);
			for (auto& fence : wait_fences) {
				vk_check(vkCreateFence(get_device(), &fence_create_info, nullptr, &fence));
			}
		}

		VkPipelineStageFlags pipeline_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		submit_info = {};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pWaitDstStageMask = &pipeline_stage_flags;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &semaphores.present_complete_semaphore;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &semaphores.render_complete_semaphore;

		// Render Pass
		VkAttachmentDescription color_attachment_desc = {};
		// Color attachment
		color_attachment_desc.format = color_format;
		color_attachment_desc.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment_desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment_desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment_desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment_desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_reference = {};
		color_reference.attachment = 0;
		color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_reference = {};
		depth_reference.attachment = 1;
		depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass_description = {};
		subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_description.colorAttachmentCount = 1;
		subpass_description.pColorAttachments = &color_reference;
		subpass_description.inputAttachmentCount = 0;
		subpass_description.pInputAttachments = nullptr;
		subpass_description.preserveAttachmentCount = 0;
		subpass_description.pPreserveAttachments = nullptr;
		subpass_description.pResolveAttachments = nullptr;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo render_pass_info = {};
		render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_info.attachmentCount = 1;
		render_pass_info.pAttachments = &color_attachment_desc;
		render_pass_info.subpassCount = 1;
		render_pass_info.pSubpasses = &subpass_description;
		render_pass_info.dependencyCount = 1;
		render_pass_info.pDependencies = &dependency;

		vk_check(vkCreateRenderPass(get_device(), &render_pass_info, nullptr, &render_pass));

		// Create framebuffers for every swapchain image
		{
			for (auto& framebuffer : framebuffers)
				vkDestroyFramebuffer(get_device(), framebuffer, nullptr);

			VkFramebufferCreateInfo framebuffer_create_info = {};
			framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_create_info.renderPass = render_pass;
			framebuffer_create_info.attachmentCount = 1;
			framebuffer_create_info.width = width;
			framebuffer_create_info.height = height;
			framebuffer_create_info.layers = 1;

			framebuffers.resize(image_count);
			for (uint32_t i = 0; i < framebuffers.size(); i++) {
				framebuffer_create_info.pAttachments = &images[i].view;
				vk_check(vkCreateFramebuffer(get_device(), &framebuffer_create_info, nullptr, &framebuffers[i]));
			}
		}
	}

	void VulkanSwapchain::destroy()
	{
		auto vk_device = get_device();
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
		vkDeviceWaitIdle(get_device());
		create(&resize_w, &resize_h, is_vsync);
		vkDeviceWaitIdle(get_device());
	}

	void VulkanSwapchain::begin_frame()
	{
		// Resource release queue
		auto& queue = Renderer::get_render_resource_free_queue(current_buffer_index);
		queue.execute();

		current_image_index = acquire_next_image();

		vk_check(vkResetCommandPool(get_device(), command_buffers[current_buffer_index].command_pool, 0));
	}

	void VulkanSwapchain::present()
	{

		VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo present_submit_info = {};
		present_submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		present_submit_info.pWaitDstStageMask = &wait_stage_mask;
		present_submit_info.pWaitSemaphores = &semaphores.present_complete_semaphore;
		present_submit_info.waitSemaphoreCount = 1;
		present_submit_info.pSignalSemaphores = &semaphores.render_complete_semaphore;
		present_submit_info.signalSemaphoreCount = 1;
		present_submit_info.pCommandBuffers = &command_buffers[current_buffer_index].buffer;
		present_submit_info.commandBufferCount = 1;

		vk_check(vkResetFences(get_device(), 1, &wait_fences[current_buffer_index]));
		vk_check(
			vkQueueSubmit(VulkanContext::get_current_device()->get_graphics_queue(), 1, &present_submit_info, wait_fences[current_buffer_index]));

		VkResult result;
		{
			VkPresentInfoKHR present_info = {};
			present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present_info.pNext = nullptr;
			present_info.swapchainCount = 1;
			present_info.pSwapchains = &swapchain;
			present_info.pImageIndices = &current_image_index;

			present_info.pWaitSemaphores = &semaphores.render_complete_semaphore;
			present_info.waitSemaphoreCount = 1;
			result = vkQueuePresentKHR(VulkanContext::get_current_device()->get_graphics_queue(), &present_info);
		}

		if (result != VK_SUCCESS) {
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				on_resize(width, height);
				return;
			} else {
				vk_check(result);
			}
		}

		{
			const auto& config = Renderer::get_config();
			current_buffer_index = (current_buffer_index + 1) % config.frames_in_flight;
			// Make sure the frame we're requesting has finished rendering
			vk_check(vkWaitForFences(get_device(), 1, &wait_fences[current_buffer_index], VK_TRUE, default_fence_timeout));
		}
	}

	uint32_t VulkanSwapchain::acquire_next_image()
	{
		uint32_t image_index;
		vk_check(vkAcquireNextImageKHR(
			get_device(), swapchain, default_fence_timeout, semaphores.present_complete_semaphore, (VkFence) nullptr, &image_index));
		return image_index;
	}

	void VulkanSwapchain::find_image_format_and_color_space()
	{
		VkPhysicalDevice physical_device = VulkanContext::get_current_device()->get_physical_device()->get_vulkan_physical_device();

		// Get list of supported surface formats
		uint32_t format_count;
		vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr));
		core_assert_bool(format_count > 0);

		std::vector<VkSurfaceFormatKHR> surface_formats(format_count);
		vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, surface_formats.data()));

		if ((format_count == 1) && (surface_formats[0].format == VK_FORMAT_UNDEFINED)) {
			color_format = VK_FORMAT_B8G8R8A8_UNORM;
			color_space = surface_formats[0].colorSpace;
		} else {
			bool found_B8G8R8A8_UNORM = false;
			for (auto&& surfaceFormat : surface_formats) {
				if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM) {
					color_format = surfaceFormat.format;
					color_space = surfaceFormat.colorSpace;
					found_B8G8R8A8_UNORM = true;
					break;
				}
			}

			if (!found_B8G8R8A8_UNORM) {
				color_format = surface_formats[0].format;
				color_space = surface_formats[0].colorSpace;
			}
		}
	}

} // namespace ForgottenEngine
