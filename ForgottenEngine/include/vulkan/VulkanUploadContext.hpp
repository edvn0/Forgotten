#pragma once

#include "VulkanContext.hpp"
#include "VulkanInitializers.hpp"

#include <vulkan/vulkan.h>

namespace ForgottenEngine {

	class UploadContext {
	public:
		VkFence upload_fence { nullptr };
		VkCommandPool upload_pool { nullptr };
		VkCommandBuffer upload_command_buffer { nullptr };

		UploadContext() = default;

		void immediate_submit(std::function<void(VkCommandBuffer)>&& func)
		{
			VkCommandBuffer cmd = upload_command_buffer;

			// begin the command buffer recording. We will use this command buffer exactly once before resetting, so we
			// tell vulkan that
			VkCommandBufferBeginInfo bi = VI::Upload::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

			VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

			// execute the function
			func(cmd);

			VK_CHECK(vkEndCommandBuffer(cmd));

			VkSubmitInfo submit = VI::Upload::submit_info(&cmd);

			// submit command buffer to the queue and execute it.
			//  _uploadFence will now block until the graphic commands finish execution
			VK_CHECK(vkQueueSubmit(VulkanContext::get_current_device()->get_graphics_queue(), 1, &submit, upload_fence));

			vkWaitForFences(VulkanContext::get_current_device()->get_vulkan_device(), 1, &upload_fence, true, 9999999999);
			vkResetFences(VulkanContext::get_current_device()->get_vulkan_device(), 1, &upload_fence);

			// reset the command buffers inside the command pool
			vkResetCommandPool(VulkanContext::get_current_device()->get_vulkan_device(), upload_pool, 0);
		}
	};

} // namespace ForgottenEngine