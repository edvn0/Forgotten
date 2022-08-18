//
//  VulkanSwapchain.hpp
//  Forgotten
//
//  Created by Edwin Carlsson on 2022-07-25.
//

#pragma once

#include "Common.hpp"
#include "render/RenderCommandBuffer.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/VulkanDevice.hpp"

#include <vector>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace ForgottenEngine {

	class VulkanSwapchain {
	public:
		VulkanSwapchain() = default;

		void init(VkInstance instance);
		void init_surface(GLFWwindow*);
		void create(uint32_t* width, uint32_t* height, bool vsync);
		void destroy();

		void on_resize(uint32_t width, uint32_t height);

		void begin_frame();
		void present();

		uint32_t get_image_count() const { return image_count; }

		uint32_t get_width() const { return width; }
		uint32_t get_height() const { return height; }

		VkRenderPass get_render_pass() { return render_pass; }

		VkFramebuffer get_current_framebuffer() { return get_framebuffer(current_image_index); }
		VkCommandBuffer get_current_drawbuffer() { return get_drawbuffer(current_buffer_index); }

		VkFormat get_color_format() { return color_format; }

		uint32_t get_current_buffer_index() const { return current_buffer_index; }

		VkFramebuffer get_framebuffer(uint32_t index)
		{
			CORE_ASSERT_BOOL(index < framebuffers.size());
			return framebuffers[index];
		}

		VkCommandBuffer get_drawbuffer(uint32_t index)
		{
			CORE_ASSERT_BOOL(index < command_buffers.size());
			return command_buffers[index].buffer;
		}

		VkSemaphore get_render_complete_semaphore() const { return semaphores.render_complete_semaphore; }

		void set_vsync(const bool enabled) { is_vsync = enabled; }

	private:
		uint32_t acquire_next_image();

		void find_image_format_and_color_space();

	private:
		VkInstance instance = nullptr;
		Reference<VulkanDevice> device;
		bool is_vsync = false;

		VkFormat color_format = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
		VkColorSpaceKHR color_space = VkColorSpaceKHR::VK_COLORSPACE_SRGB_NONLINEAR_KHR;

		VkSwapchainKHR swapchain = nullptr;
		uint32_t image_count = 0;
		std::vector<VkImage> vulkan_images;

		struct SwapchainImage {
			VkImage image = nullptr;
			VkImageView view = nullptr;
		};
		std::vector<SwapchainImage> images;

		struct {
			VkImage image = nullptr;
			VmaAllocation memory = nullptr;
			VkImageView view = nullptr;
		} depth_stencil;

		std::vector<VkFramebuffer> framebuffers;

		struct SwapchainCommandBuffer {
			VkCommandPool command_pool = nullptr;
			VkCommandBuffer buffer = nullptr;
		};
		std::vector<SwapchainCommandBuffer> command_buffers;

		struct {
			// Swap chain
			VkSemaphore present_complete_semaphore = nullptr;
			// Command buffer
			VkSemaphore render_complete_semaphore = nullptr;
		} semaphores;
		VkSubmitInfo submit_info {};

		std::vector<VkFence> wait_fences;

		VkRenderPass render_pass = nullptr;
		uint32_t current_buffer_index = 0;
		uint32_t current_image_index = 0;

		uint32_t queue_node_index = UINT32_MAX;
		uint32_t width = 0, height = 0;

		VkSurfaceKHR surface { nullptr };

		friend class VulkanContext;
	};

} // namespace ForgottenEngine
